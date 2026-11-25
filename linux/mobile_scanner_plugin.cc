#include <mobile_scanner/mobile_scanner_plugin.h>

#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include <flutter_linux/flutter_linux.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "../src/ms_frame_crop.h"
#include "../src/ms_zxing.h"

typedef struct _CameraTexture CameraTexture;
typedef struct _CameraTextureClass CameraTextureClass;
struct _CameraTexture {
  FlPixelBufferTexture parent_instance;
  std::mutex* mutex;
  std::vector<uint8_t>* pixels;
  std::vector<uint8_t>* front;
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
  if (self->pixels->empty() || !self->width || !self->height) {
    // Camera still warming up: hand the engine a 1x1 black pixel.
    static const uint8_t black[4] = {0, 0, 0, 255};
    *buffer = black; *width = 1; *height = 1;
    return TRUE;
  }
  *self->front = *self->pixels;  // stable copy; decoder thread may replace pixels
  *buffer = self->front->data(); *width = self->width; *height = self->height;
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
  delete self->front;
  delete self->mutex;
  G_OBJECT_CLASS(camera_texture_parent_class)->finalize(object);
}
static void camera_texture_init(CameraTexture* self) {
  self->mutex = new std::mutex();
  self->pixels = new std::vector<uint8_t>();
  self->front = new std::vector<uint8_t>();
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
  gint frames_seen;
  uint64_t format_mask;
  // Zoom, torch and scan window, written by the method handler and read by the
  // capture thread. The window and zoom share one mutex so they stay consistent.
  GMutex state_mutex;
  gdouble zoom;
  gboolean has_window;
  gdouble window[4];  // left, top, right, bottom as fractions of the frame.
  gint torch_on;
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

struct SendEventData { MobileScannerPlugin* self; FlValue* event; };
static gboolean send_event_main(gpointer data) {
  auto* d = static_cast<SendEventData*>(data);
  if (d->self->events && g_atomic_int_get(&d->self->active))
    fl_event_channel_send(d->self->events, d->event, nullptr, nullptr);
  fl_value_unref(d->event);
  g_object_unref(d->self);
  delete d;
  return G_SOURCE_REMOVE;
}

static GstFlowReturn on_frame(GstAppSink* sink, gpointer data) {
  auto* self = MOBILE_SCANNER_PLUGIN(data);
  GstSample* sample = gst_app_sink_pull_sample(sink);
  if (!sample) return GST_FLOW_EOS;
  GstCaps* caps = gst_sample_get_caps(sample);
  GstVideoInfo info;
  GstBuffer* gst_buffer = gst_sample_get_buffer(sample);
  // Webcams emit corrupt/half-exposed frames while warming up: skip those.
  if (g_atomic_int_add(&self->frames_seen, 1) < 15 ||
      GST_BUFFER_FLAG_IS_SET(gst_buffer, GST_BUFFER_FLAG_CORRUPTED)) {
    gst_sample_unref(sample); return GST_FLOW_OK;
  }
  GstMapInfo map;
  if (!gst_video_info_from_caps(&info, caps) || !gst_buffer_map(gst_buffer, &map, GST_MAP_READ)) {
    gst_sample_unref(sample); return GST_FLOW_OK;
  }
  const uint32_t source_width = GST_VIDEO_INFO_WIDTH(&info), source_height = GST_VIDEO_INFO_HEIGHT(&info);
  const int stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);
  std::vector<uint8_t> full(static_cast<size_t>(source_width) * source_height * 4);
  for (uint32_t y = 0; y < source_height; ++y)
    std::memcpy(full.data() + static_cast<size_t>(y) * source_width * 4,
                map.data + static_cast<size_t>(y) * stride, source_width * 4);
  gst_buffer_unmap(gst_buffer, &map);
  gst_sample_unref(sample);

  // Zoom and scan window as last set by the method handler.
  g_mutex_lock(&self->state_mutex);
  const double zoom = self->zoom;
  const bool has_window = self->has_window;
  double window[4] = {0, 0, 1, 1};
  if (has_window) std::copy(self->window, self->window + 4, window);
  g_mutex_unlock(&self->state_mutex);

  // Digital zoom: the preview and the decoder both use the centred crop.
  const ms_frame::Rect zoom_rect = ms_frame::ZoomRect(source_width, source_height, zoom);
  const bool zoomed = zoom_rect.width != source_width || zoom_rect.height != source_height;
  std::vector<uint8_t> frame = zoomed ? ms_frame::CopyRect(full.data(), source_width, zoom_rect)
                                      : std::move(full);
  const uint32_t width = zoom_rect.width, height = zoom_rect.height;

  if (self->texture) {
    std::lock_guard<std::mutex> lock(*self->texture->mutex);
    *self->texture->pixels = frame; self->texture->width = width; self->texture->height = height;
    fl_texture_registrar_mark_texture_frame_available(self->textures, FL_TEXTURE(self->texture));
  }
  static gint64 last_decode = 0;
  const gint64 now = g_get_monotonic_time();
  if (g_atomic_int_get(&self->active) && self->events && now - last_decode > 100000) {
    last_decode = now;
    MsZxingDecodeParams params{};
    params.data = frame.data(); params.width = width; params.height = height;
    params.row_stride = width * 4; params.pixel_format = MS_ZXING_PIXEL_RGBA;
    params.format_mask = self->format_mask; params.try_harder = 0;
    params.max_symbols = 10;
    int32_t crop[4];
    ms_frame::ScanWindowCrop(has_window ? window : nullptr, width, height, crop);
    params.crop_left = crop[0]; params.crop_top = crop[1];
    params.crop_width = crop[2]; params.crop_height = crop[3];
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
      g_idle_add(send_event_main, new SendEventData{MOBILE_SCANNER_PLUGIN(g_object_ref(self)), event});
    }
    if (result) ms_zxing_free_results(result);
  }
  return GST_FLOW_OK;
}

// Sends {name, data} on the event channel. Runs on the platform thread.
static void send_event(MobileScannerPlugin* self, const char* name, FlValue* data) {
  if (!self->events) { fl_value_unref(data); return; }
  FlValue* event = fl_value_new_map();
  fl_value_set_string_take(event, "name", fl_value_new_string(name));
  fl_value_set_string_take(event, "data", data);
  fl_event_channel_send(self->events, event, nullptr, nullptr);
  fl_value_unref(event);
}

static void set_zoom(MobileScannerPlugin* self, double zoom) {
  zoom = std::clamp(zoom, 0.0, 1.0);
  g_mutex_lock(&self->state_mutex);
  self->zoom = zoom;
  g_mutex_unlock(&self->state_mutex);
  send_event(self, "zoomScaleState", fl_value_new_float(zoom));
}

// Video devices are opened read-write for control requests. GStreamer's
// v4l2src opens the same node for streaming, which V4L2 allows.
static const char kVideoDevice[] = "/dev/video0";

static bool v4l2_control_exists(int fd, uint32_t id) {
  struct v4l2_queryctrl query {};
  query.id = id;
  return ioctl(fd, VIDIOC_QUERYCTRL, &query) == 0 &&
         !(query.flags & V4L2_CTRL_FLAG_DISABLED);
}

static bool v4l2_set_control(int fd, uint32_t id, int32_t value) {
  struct v4l2_control control {};
  control.id = id;
  control.value = value;
  return ioctl(fd, VIDIOC_S_CTRL, &control) == 0;
}

// True when the camera exposes the given V4L2 control. Used to report
// capabilities at start, so the UI only offers controls that really exist.
static bool camera_has_control(uint32_t id) {
  const int fd = open(kVideoDevice, O_RDWR | O_CLOEXEC);
  if (fd < 0) return false;
  const bool present = v4l2_control_exists(fd, id);
  close(fd);
  return present;
}

// Returns false when the camera has no torch (flash LED) control.
static bool set_torch(bool on) {
  const int fd = open(kVideoDevice, O_RDWR | O_CLOEXEC);
  if (fd < 0) return false;
  const bool ok = v4l2_control_exists(fd, V4L2_CID_FLASH_LED_MODE) &&
                  v4l2_set_control(fd, V4L2_CID_FLASH_LED_MODE,
                                   on ? V4L2_FLASH_LED_MODE_TORCH : V4L2_FLASH_LED_MODE_NONE);
  close(fd);
  return ok;
}

// Re-runs autofocus. V4L2 drivers cannot focus on a point, so the requested
// position is ignored. Returns false when the camera has no autofocus control.
static bool trigger_autofocus() {
  const int fd = open(kVideoDevice, O_RDWR | O_CLOEXEC);
  if (fd < 0) return false;
  const bool ok = v4l2_control_exists(fd, V4L2_CID_FOCUS_AUTO) &&
                  v4l2_set_control(fd, V4L2_CID_FOCUS_AUTO, 0) &&
                  v4l2_set_control(fd, V4L2_CID_FOCUS_AUTO, 1);
  close(fd);
  return ok;
}

static void on_method_call(FlMethodChannel*, FlMethodCall* call, gpointer data) {
  auto* self = MOBILE_SCANNER_PLUGIN(data);
  const char* method = fl_method_call_get_name(call);
  FlValue* args = fl_method_call_get_args(call);
  if (strcmp(method, "state") == 0) { fl_method_call_respond_success(call, fl_value_new_int(1), nullptr); }
  else if (strcmp(method, "request") == 0) { fl_method_call_respond_success(call, fl_value_new_bool(TRUE), nullptr); }
  else if (strcmp(method, "start") == 0) {
    stop_camera(self);
    GstElement* pipeline = gst_parse_launch("v4l2src device=/dev/video0 ! decodebin ! videoconvert ! videoscale ! video/x-raw,format=RGBA ! appsink name=scanner_sink emit-signals=true max-buffers=1 drop=true", nullptr);
    if (!pipeline) { fl_method_call_respond_error(call, "camera_unavailable", "Could not create GStreamer camera pipeline", nullptr, nullptr); return; }
    auto* sink = gst_bin_get_by_name(GST_BIN(pipeline), "scanner_sink");
    g_signal_connect(sink, "new-sample", G_CALLBACK(on_frame), self);
    gst_object_unref(sink);
    self->texture = reinterpret_cast<CameraTexture*>(g_object_new(camera_texture_get_type(), nullptr));
    self->pipeline = pipeline; g_atomic_int_set(&self->frames_seen, 0); g_atomic_int_set(&self->active, TRUE);
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
        bus, 500 * GST_MSECOND, static_cast<GstMessageType>(GST_MESSAGE_ERROR));
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
    // Report only the controls this camera really has. A missing torch is
    // reported as "unavailable" (-1), which the Dart side maps to hidden.
    const bool has_torch = camera_has_control(V4L2_CID_FLASH_LED_MODE);
    const bool has_autofocus = camera_has_control(V4L2_CID_FOCUS_AUTO);
    g_atomic_int_set(&self->torch_on, FALSE);
    fl_value_set_string_take(result,"currentTorchState",fl_value_new_int(has_torch ? 0 : -1));
    FlValue* capabilities = fl_value_new_map();
    fl_value_set_string_take(capabilities,"torch",fl_value_new_bool(has_torch));
    fl_value_set_string_take(capabilities,"zoom",fl_value_new_bool(TRUE));  // Digital zoom.
    fl_value_set_string_take(capabilities,"focus",fl_value_new_bool(has_autofocus));
    fl_value_set_string_take(result,"capabilities",capabilities);
    fl_value_set_string_take(result,"numberOfCameras",fl_value_new_int(1));
    FlValue* size=fl_value_new_map(); fl_value_set_string_take(size,"width",fl_value_new_float(1280)); fl_value_set_string_take(size,"height",fl_value_new_float(720));
    fl_value_set_string_take(result,"size",size);
    fl_method_call_respond_success(call,result,nullptr);
  } else if (strcmp(method,"stop")==0 || strcmp(method,"pause")==0) { stop_camera(self); fl_method_call_respond_success(call,nullptr,nullptr); }
  else if (strcmp(method,"getSupportedLenses")==0) { fl_method_call_respond_success(call,fl_value_new_list(),nullptr); }
  else if (strcmp(method,"getBestCloseRangeScanningLens")==0) { fl_method_call_respond_success(call,fl_value_new_int(0),nullptr); }
  else if (strcmp(method, "setScale") == 0) {
    const double zoom = (args && fl_value_get_type(args) == FL_VALUE_TYPE_FLOAT) ? fl_value_get_float(args) : 0.0;
    set_zoom(self, zoom);
    fl_method_call_respond_success(call, nullptr, nullptr);
  } else if (strcmp(method, "resetScale") == 0) {
    set_zoom(self, 0.0);
    fl_method_call_respond_success(call, nullptr, nullptr);
  } else if (strcmp(method, "toggleTorch") == 0) {
    const bool was_on = g_atomic_int_get(&self->torch_on) != 0;
    if (!set_torch(!was_on)) {
      fl_method_call_respond_error(call, "unsupported", "This camera has no torch control.", nullptr, nullptr);
    } else {
      g_atomic_int_set(&self->torch_on, !was_on);
      send_event(self, "torchState", fl_value_new_int(was_on ? 0 : 1));
      fl_method_call_respond_success(call, nullptr, nullptr);
    }
  } else if (strcmp(method, "setFocus") == 0) {
    // Webcams cannot focus on a point. Re-running autofocus is the closest
    // equivalent, and a camera without autofocus simply keeps its focus.
    trigger_autofocus();
    fl_method_call_respond_success(call, nullptr, nullptr);
  } else if (strcmp(method, "updateScanWindow") == 0) {
    FlValue* rect = (args && fl_value_get_type(args) == FL_VALUE_TYPE_MAP)
                        ? fl_value_lookup_string(args, "rect") : nullptr;
    const bool has_window = rect && fl_value_get_type(rect) == FL_VALUE_TYPE_LIST &&
                            fl_value_get_length(rect) == 4;
    g_mutex_lock(&self->state_mutex);
    self->has_window = has_window;
    if (has_window) {
      for (int i = 0; i < 4; ++i) {
        FlValue* value = fl_value_get_list_value(rect, i);
        self->window[i] = fl_value_get_type(value) == FL_VALUE_TYPE_FLOAT
                              ? fl_value_get_float(value)
                              : static_cast<double>(fl_value_get_int(value));
      }
    }
    g_mutex_unlock(&self->state_mutex);
    fl_method_call_respond_success(call, nullptr, nullptr);
  }
  else { fl_method_call_respond_not_implemented(call,nullptr); }
}

static FlMethodErrorResponse* on_event_listen(FlEventChannel*, FlValue*, gpointer) { return nullptr; }
static FlMethodErrorResponse* on_event_cancel(FlEventChannel*, FlValue*, gpointer) { return nullptr; }
static FlMethodErrorResponse* on_orientation_listen(FlEventChannel* channel, FlValue*, gpointer) { fl_event_channel_send(channel,fl_value_new_string("PORTRAIT_UP"),nullptr,nullptr); return nullptr; }
static FlMethodErrorResponse* on_orientation_cancel(FlEventChannel*, FlValue*, gpointer) { return nullptr; }
static void mobile_scanner_plugin_dispose(GObject* object) { auto* self=MOBILE_SCANNER_PLUGIN(object); stop_camera(self); g_mutex_clear(&self->state_mutex); g_clear_object(&self->methods); g_clear_object(&self->events); g_clear_object(&self->orientation); g_clear_object(&self->textures); G_OBJECT_CLASS(mobile_scanner_plugin_parent_class)->dispose(object); }
static void mobile_scanner_plugin_class_init(MobileScannerPluginClass* klass) { G_OBJECT_CLASS(klass)->dispose=mobile_scanner_plugin_dispose; }
static void mobile_scanner_plugin_init(MobileScannerPlugin* self) {
  gst_init(nullptr, nullptr);
  g_mutex_init(&self->state_mutex);
}
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
