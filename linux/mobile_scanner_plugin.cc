#include <mobile_scanner/mobile_scanner_plugin.h>

#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include <flutter_linux/flutter_linux.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>
#include "../src/ms_zxing.h"

typedef struct _CameraTexture CameraTexture;
typedef struct _CameraTextureClass CameraTextureClass;
struct _CameraTexture {
  FlPixelBufferTexture parent_instance;
  std::mutex* mutex;
  std::vector<uint8_t>* pixels;
  uint32_t width, height;
};
struct _CameraTextureClass {
  FlPixelBufferTextureClass parent_class;
};
G_DEFINE_TYPE(CameraTexture, camera_texture, fl_pixel_buffer_texture_get_type())

static gboolean camera_texture_copy(FlPixelBufferTexture* base,
                                    const uint8_t** buffer, uint32_t* width,
                                    uint32_t* height, GError**) {
  auto* self = reinterpret_cast<CameraTexture*>(base);
  std::lock_guard<std::mutex> lock(*self->mutex);
  if (self->pixels->empty()) return FALSE;
  *buffer = self->pixels->data(); *width = self->width; *height = self->height;
  return TRUE;
}
static void camera_texture_finalize(GObject* object);
static void camera_texture_class_init(CameraTextureClass* klass) {
  FL_PIXEL_BUFFER_TEXTURE_CLASS(klass)->copy_pixels = camera_texture_copy;
  G_OBJECT_CLASS(klass)->finalize = camera_texture_finalize;
}
static void camera_texture_finalize(GObject* object) {
  auto* self = reinterpret_cast<CameraTexture*>(object);
  delete self->pixels;
  delete self->mutex;
  G_OBJECT_CLASS(camera_texture_parent_class)->finalize(object);
}
static void camera_texture_init(CameraTexture* self) {
  self->mutex = new std::mutex();
  self->pixels = new std::vector<uint8_t>();
  self->width = self->height = 0;
}

struct _MobileScannerPlugin {
  GObject parent_instance;
  FlTextureRegistrar* textures;
  FlMethodChannel* methods;
  FlEventChannel* events;
  FlEventChannel* orientation;
  CameraTexture* texture;
  GstElement* pipeline;
  int64_t texture_id;
  gint active;
  uint64_t format_mask;
};
G_DEFINE_TYPE(MobileScannerPlugin, mobile_scanner_plugin, G_TYPE_OBJECT)

static void stop_camera(MobileScannerPlugin* self) {
  g_atomic_int_set(&self->active, FALSE);
  if (self->pipeline) {
    gst_element_set_state(self->pipeline, GST_STATE_NULL);
    gst_object_unref(self->pipeline); self->pipeline = nullptr;
  }
  if (self->texture) {
    fl_texture_registrar_unregister_texture(self->textures, FL_TEXTURE(self->texture));
    g_clear_object(&self->texture);
  }
}

static GstFlowReturn on_frame(GstAppSink* sink, gpointer data) {
  auto* self = MOBILE_SCANNER_PLUGIN(data);
  GstSample* sample = gst_app_sink_pull_sample(sink);
  if (!sample) return GST_FLOW_EOS;
  GstCaps* caps = gst_sample_get_caps(sample);
  GstVideoInfo info;
  GstBuffer* gst_buffer = gst_sample_get_buffer(sample);
  GstMapInfo map;
  if (!gst_video_info_from_caps(&info, caps) || !gst_buffer_map(gst_buffer, &map, GST_MAP_READ)) {
    gst_sample_unref(sample); return GST_FLOW_OK;
  }
  const uint32_t width = GST_VIDEO_INFO_WIDTH(&info), height = GST_VIDEO_INFO_HEIGHT(&info);
  const int stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);
  std::vector<uint8_t> frame(static_cast<size_t>(width) * height * 4);
  for (uint32_t y = 0; y < height; ++y)
    std::memcpy(frame.data() + static_cast<size_t>(y) * width * 4,
                map.data + static_cast<size_t>(y) * stride, width * 4);
  gst_buffer_unmap(gst_buffer, &map);
  gst_sample_unref(sample);
  if (self->texture) {
    std::lock_guard<std::mutex> lock(*self->texture->mutex);
    *self->texture->pixels = frame; self->texture->width = width; self->texture->height = height;
    fl_texture_registrar_mark_texture_frame_available(self->textures, FL_TEXTURE(self->texture));
  }
  if (g_atomic_int_get(&self->active) && self->events) {
    MsZxingDecodeParams params{};
    params.data = frame.data(); params.width = width; params.height = height;
    params.row_stride = width * 4; params.pixel_format = MS_ZXING_PIXEL_RGBA;
    params.format_mask = self->format_mask; params.try_harder = 0;
    params.max_symbols = 10;
    auto* result = ms_zxing_decode_sized_v2(&params, frame.size());
    if (result && result->count > 0) {
      FlValue* list = fl_value_new_list();
      for (int i = 0; i < result->count; ++i) {
        const auto& barcode = result->results[i];
        FlValue* item = fl_value_new_map();
        fl_value_set_string_take(item, "format", fl_value_new_int(barcode.format));
        fl_value_set_string_take(item, "rawValue", fl_value_new_string(barcode.text ? barcode.text : ""));
        fl_value_set_string_take(item, "type", fl_value_new_int(0));
        FlValue* size = fl_value_new_map();
        float left = barcode.corners[0].x, right = left;
        float top = barcode.corners[0].y, bottom = top;
        for (const auto& point : barcode.corners) {
          left = std::min(left, point.x); right = std::max(right, point.x);
          top = std::min(top, point.y); bottom = std::max(bottom, point.y);
        }
        fl_value_set_string_take(size, "width", fl_value_new_float(right-left));
        fl_value_set_string_take(size, "height", fl_value_new_float(bottom-top));
        fl_value_set_string_take(item, "size", size);
        FlValue* corners = fl_value_new_list();
        for (const auto& point : barcode.corners) {
          FlValue* p = fl_value_new_map();
          fl_value_set_string_take(p, "x", fl_value_new_float(point.x));
          fl_value_set_string_take(p, "y", fl_value_new_float(point.y));
          fl_value_append_take(corners, p);
        }
        fl_value_set_string_take(item, "corners", corners);
        fl_value_append_take(list, item);
      }
      FlValue* event = fl_value_new_map();
      fl_value_set_string_take(event, "name", fl_value_new_string("barcode"));
      fl_value_set_string_take(event, "data", list);
      FlValue* image = fl_value_new_map();
      fl_value_set_string_take(image, "width", fl_value_new_float(width));
      fl_value_set_string_take(image, "height", fl_value_new_float(height));
      fl_value_set_string_take(event, "image", image);
      fl_event_channel_send(self->events, event, nullptr, nullptr);
      ms_zxing_free_results(result);
    }
  }
  return GST_FLOW_OK;
}

static void on_method_call(FlMethodChannel*, FlMethodCall* call, gpointer data) {
  auto* self = MOBILE_SCANNER_PLUGIN(data);
  const char* method = fl_method_call_get_name(call);
  FlValue* args = fl_method_call_get_args(call);
  if (strcmp(method, "state") == 0) { fl_method_call_respond_success(call, fl_value_new_int(1), nullptr); }
  else if (strcmp(method, "request") == 0) { fl_method_call_respond_success(call, fl_value_new_bool(TRUE), nullptr); }
  else if (strcmp(method, "start") == 0) {
    stop_camera(self);
    GstElement* pipeline = gst_parse_launch("v4l2src ! videoconvert ! video/x-raw,format=RGBA,width=1280,height=720 ! appsink name=scanner_sink emit-signals=true max-buffers=1 drop=true", nullptr);
    if (!pipeline) { fl_method_call_respond_error(call, "camera_unavailable", "Could not create GStreamer camera pipeline", nullptr, nullptr); return; }
    auto* sink = gst_bin_get_by_name(GST_BIN(pipeline), "scanner_sink");
    g_signal_connect(sink, "new-sample", G_CALLBACK(on_frame), self);
    gst_object_unref(sink);
    self->texture = reinterpret_cast<CameraTexture*>(g_object_new(camera_texture_get_type(), nullptr));
    self->pipeline = pipeline; g_atomic_int_set(&self->active, TRUE);
    fl_texture_registrar_register_texture(self->textures, FL_TEXTURE(self->texture));
    self->texture_id = fl_texture_get_id(FL_TEXTURE(self->texture));
    if (args && fl_value_get_type(args) == FL_VALUE_TYPE_MAP) {
      FlValue* formats = fl_value_lookup_string(args, "formats");
      if (formats && fl_value_get_type(formats) == FL_VALUE_TYPE_LIST) {
        self->format_mask = 0;
        for (size_t i=0; i<fl_value_get_length(formats); ++i) self->format_mask |= static_cast<uint64_t>(fl_value_get_int(fl_value_get_list_value(formats, i)));
      }
    }
    const GstStateChangeReturn state_result = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    GstBus* bus = gst_element_get_bus(pipeline);
    GstMessage* error_message = gst_bus_timed_pop_filtered(
        bus, 2 * GST_SECOND, static_cast<GstMessageType>(GST_MESSAGE_ERROR));
    gst_object_unref(bus);
    if (state_result == GST_STATE_CHANGE_FAILURE || error_message) {
      gchar* message = nullptr;
      if (error_message) {
        GError* error = nullptr;
        gst_message_parse_error(error_message, &error, nullptr);
        if (error) { message = g_strdup(error->message); g_error_free(error); }
        gst_message_unref(error_message);
      }
      stop_camera(self);
      fl_method_call_respond_error(call, "camera_unavailable",
          message ? message : "Could not start the default V4L2 camera", nullptr, nullptr);
      g_free(message);
      return;
    }
    FlValue* result = fl_value_new_map();
    fl_value_set_string_take(result,"textureId",fl_value_new_int(self->texture_id));
    fl_value_set_string_take(result,"cameraDirection",fl_value_new_int(1));
    fl_value_set_string_take(result,"currentTorchState",fl_value_new_int(0));
    fl_value_set_string_take(result,"numberOfCameras",fl_value_new_int(1));
    FlValue* size=fl_value_new_map(); fl_value_set_string_take(size,"width",fl_value_new_float(1280)); fl_value_set_string_take(size,"height",fl_value_new_float(720));
    fl_value_set_string_take(result,"size",size);
    fl_method_call_respond_success(call,result,nullptr);
  } else if (strcmp(method,"stop")==0 || strcmp(method,"pause")==0) { stop_camera(self); fl_method_call_respond_success(call,nullptr,nullptr); }
  else if (strcmp(method,"getSupportedLenses")==0) { fl_method_call_respond_success(call,fl_value_new_list(),nullptr); }
  else if (strcmp(method,"getBestCloseRangeScanningLens")==0) { fl_method_call_respond_success(call,fl_value_new_int(0),nullptr); }
  else if (strcmp(method,"setScale")==0 || strcmp(method,"resetScale")==0 || strcmp(method,"toggleTorch")==0 || strcmp(method,"setFocus")==0 || strcmp(method,"updateScanWindow")==0) { fl_method_call_respond_success(call,nullptr,nullptr); }
  else { fl_method_call_respond_not_implemented(call,nullptr); }
}

static FlMethodErrorResponse* on_event_listen(FlEventChannel*, FlValue*, gpointer) { return nullptr; }
static FlMethodErrorResponse* on_event_cancel(FlEventChannel*, FlValue*, gpointer) { return nullptr; }
static FlMethodErrorResponse* on_orientation_listen(FlEventChannel* channel, FlValue*, gpointer) { fl_event_channel_send(channel,fl_value_new_string("portraitUp"),nullptr,nullptr); return nullptr; }
static FlMethodErrorResponse* on_orientation_cancel(FlEventChannel*, FlValue*, gpointer) { return nullptr; }
static void mobile_scanner_plugin_dispose(GObject* object) { auto* self=MOBILE_SCANNER_PLUGIN(object); stop_camera(self); g_clear_object(&self->methods); g_clear_object(&self->events); g_clear_object(&self->orientation); g_clear_object(&self->textures); G_OBJECT_CLASS(mobile_scanner_plugin_parent_class)->dispose(object); }
static void mobile_scanner_plugin_class_init(MobileScannerPluginClass* klass) { G_OBJECT_CLASS(klass)->dispose=mobile_scanner_plugin_dispose; }
static void mobile_scanner_plugin_init(MobileScannerPlugin*) { gst_init(nullptr,nullptr); }
void mobile_scanner_plugin_register_with_registrar(FlPluginRegistrar* registrar) {
  auto* self=MOBILE_SCANNER_PLUGIN(g_object_new(mobile_scanner_plugin_get_type(),nullptr));
  self->textures=FL_TEXTURE_REGISTRAR(g_object_ref(fl_plugin_registrar_get_texture_registrar(registrar)));
  self->methods=fl_method_channel_new(fl_plugin_registrar_get_messenger(registrar),"dev.steenbakker.mobile_scanner/scanner/method",FL_METHOD_CODEC(fl_standard_method_codec_new()));
  fl_method_channel_set_method_call_handler(self->methods,on_method_call,g_object_ref(self),g_object_unref);
  self->events=fl_event_channel_new(fl_plugin_registrar_get_messenger(registrar),"dev.steenbakker.mobile_scanner/scanner/event",FL_METHOD_CODEC(fl_standard_method_codec_new()));
  fl_event_channel_set_stream_handlers(self->events,on_event_listen,on_event_cancel,g_object_ref(self),g_object_unref);
  self->orientation=fl_event_channel_new(fl_plugin_registrar_get_messenger(registrar),"dev.steenbakker.mobile_scanner/scanner/deviceOrientation",FL_METHOD_CODEC(fl_standard_method_codec_new()));
  fl_event_channel_set_stream_handlers(self->orientation,on_orientation_listen,on_orientation_cancel,g_object_ref(self),g_object_unref);
  g_object_unref(self);
}
