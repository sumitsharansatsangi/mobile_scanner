// Windows live camera capture for mobile_scanner.
//
// Camera frames come from Media Foundation (IMFSourceReader, RGB32 output,
// video processing enabled so MJPG/YUY2/NV12 webcams are converted). Each frame
// is published as a Flutter pixel-buffer texture (BGRA8888) and decoded with the
// shared ZXing-C++ engine (ms_zxing.dll) at a throttled rate.
//
// Threading:
//   - Method/event channel handlers and the texture registrar run on the
//     platform thread.
//   - A capture worker thread reads frames and decodes them.
//   - Barcode events are queued by the worker and delivered on the platform
//     thread via a hidden message-only window, because Flutter event sinks
//     must only be called from the platform thread.

#include <mobile_scanner/mobile_scanner_plugin.h>

#include <flutter/event_channel.h>
#include <flutter/event_stream_handler_functions.h>
#include <flutter/method_channel.h>
#include <flutter/plugin_registrar.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>
#include <flutter/texture_registrar.h>

#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <windows.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "ms_frame_crop.h"
#include "ms_zxing.h"

namespace {

using flutter::EncodableList;
using flutter::EncodableMap;
using flutter::EncodableValue;
using Microsoft::WRL::ComPtr;

constexpr UINT kDispatchMessage = WM_APP + 0x4D53;  // Pending barcode events.
constexpr wchar_t kEventWindowClass[] = L"MobileScannerEventWindow";
constexpr auto kDecodeInterval = std::chrono::milliseconds(100);
// Webcams emit corrupt or half-exposed frames while warming up.
constexpr int kWarmupFrames = 15;

// Reads an integer-valued EncodableValue (the standard codec may send int32
// or int64) as an unsigned 64-bit value.
bool ReadInteger(const EncodableValue& value, uint64_t* out) {
  if (const auto* v32 = std::get_if<int32_t>(&value)) {
    *out = static_cast<uint64_t>(*v32);
    return true;
  }
  if (const auto* v64 = std::get_if<int64_t>(&value)) {
    *out = static_cast<uint64_t>(*v64);
    return true;
  }
  return false;
}

// Builds a string EncodableValue. Passing a raw const char* to the variant
// constructor could select the bool overload, so always go through std::string.
EncodableValue Key(const char* text) { return EncodableValue(std::string(text)); }

const EncodableValue* FindKey(const EncodableMap& map, const char* key) {
  auto it = map.find(Key(key));
  return it == map.end() ? nullptr : &it->second;
}

}  // namespace

class MobileScannerPlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows* registrar);

  explicit MobileScannerPlugin(flutter::PluginRegistrarWindows* registrar);
  ~MobileScannerPlugin() override;

  MobileScannerPlugin(const MobileScannerPlugin&) = delete;
  MobileScannerPlugin& operator=(const MobileScannerPlugin&) = delete;

 private:
  using Result = flutter::MethodResult<EncodableValue>;

  void HandleMethodCall(const flutter::MethodCall<EncodableValue>& call,
                        std::unique_ptr<Result> result);
  bool StartCamera(uint64_t format_mask, EncodableMap* response,
                   std::string* error);
  void StopCamera();
  void CaptureLoop(ComPtr<IMFSourceReader> reader);
  void ProcessFrame(const uint8_t* src, DWORD src_length);
  void DecodeFrame(const std::vector<uint8_t>& frame, uint32_t width,
                   uint32_t height);
  void PostBarcodeEvent(EncodableValue event);
  void DrainPendingEvents();
  void SetZoom(double zoom);
  void SendEvent(const char* name, EncodableValue data);
  const FlutterDesktopPixelBuffer* FillPixelBuffer(size_t width, size_t height);
  static LRESULT CALLBACK EventWindowProc(HWND hwnd, UINT message, WPARAM wparam,
                                          LPARAM lparam);

  flutter::TextureRegistrar* textures_ = nullptr;
  std::unique_ptr<flutter::MethodChannel<EncodableValue>> method_channel_;
  std::unique_ptr<flutter::EventChannel<EncodableValue>> event_channel_;
  std::unique_ptr<flutter::EventChannel<EncodableValue>> orientation_channel_;
  std::unique_ptr<flutter::EventSink<EncodableValue>> event_sink_;

  HWND event_window_ = nullptr;
  bool media_foundation_started_ = false;

  // Camera state (owned by the platform thread; the worker reads a copy).
  ComPtr<IMFMediaSource> source_;
  std::thread worker_;
  std::atomic<bool> running_{false};
  uint32_t frame_width_ = 0;
  uint32_t frame_height_ = 0;
  int32_t stride_ = 0;
  uint64_t format_mask_ = 0;
  int64_t texture_id_ = -1;
  std::unique_ptr<flutter::TextureVariant> texture_;

  // Latest frame, shared between the worker (writer) and the raster thread.
  std::mutex frame_mutex_;
  std::vector<uint8_t> frame_;
  std::vector<uint8_t> texture_frame_;
  uint32_t texture_width_ = 0;
  uint32_t texture_height_ = 0;
  FlutterDesktopPixelBuffer pixel_buffer_{};

  // Barcode events queued by the worker for delivery on the platform thread.
  std::mutex event_mutex_;
  std::deque<EncodableValue> pending_events_;

  // Zoom and scan window: set on the platform thread, read by the worker.
  std::atomic<double> zoom_{0.0};
  std::mutex window_mutex_;
  bool has_window_ = false;
  double window_[4] = {0, 0, 1, 1};  // left, top, right, bottom as fractions.

  // Size of the frame currently published to the texture (guarded by frame_mutex_).
  uint32_t latest_width_ = 0;
  uint32_t latest_height_ = 0;

  // Worker-only state.
  int frames_seen_ = 0;
  std::chrono::steady_clock::time_point last_decode_{};
};

void MobileScannerPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows* registrar) {
  auto plugin = std::make_unique<MobileScannerPlugin>(registrar);
  registrar->AddPlugin(std::move(plugin));
}

MobileScannerPlugin::MobileScannerPlugin(
    flutter::PluginRegistrarWindows* registrar)
    : textures_(registrar->texture_registrar()) {
  auto* messenger = registrar->messenger();

  method_channel_ = std::make_unique<flutter::MethodChannel<EncodableValue>>(
      messenger, "dev.steenbakker.mobile_scanner/scanner/method",
      &flutter::StandardMethodCodec::GetInstance());
  method_channel_->SetMethodCallHandler(
      [this](const flutter::MethodCall<EncodableValue>& call,
             std::unique_ptr<Result> result) {
        HandleMethodCall(call, std::move(result));
      });

  event_channel_ = std::make_unique<flutter::EventChannel<EncodableValue>>(
      messenger, "dev.steenbakker.mobile_scanner/scanner/event",
      &flutter::StandardMethodCodec::GetInstance());
  event_channel_->SetStreamHandler(
      std::make_unique<flutter::StreamHandlerFunctions<EncodableValue>>(
          [this](const EncodableValue*,
                 std::unique_ptr<flutter::EventSink<EncodableValue>>&& events)
              -> std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> {
            event_sink_ = std::move(events);
            return nullptr;
          },
          [this](const EncodableValue*)
              -> std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> {
            event_sink_.reset();
            return nullptr;
          }));

  orientation_channel_ = std::make_unique<flutter::EventChannel<EncodableValue>>(
      messenger, "dev.steenbakker.mobile_scanner/scanner/deviceOrientation",
      &flutter::StandardMethodCodec::GetInstance());
  orientation_channel_->SetStreamHandler(
      std::make_unique<flutter::StreamHandlerFunctions<EncodableValue>>(
          [](const EncodableValue*,
             std::unique_ptr<flutter::EventSink<EncodableValue>>&& events)
              -> std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> {
            // Desktop devices have a fixed orientation.
            events->Success(Key("PORTRAIT_UP"));
            return nullptr;
          },
          [](const EncodableValue*)
              -> std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> {
            return nullptr;
          }));

  // Hidden message-only window: gives barcode events a platform-thread
  // dispatch point.
  WNDCLASSEXW window_class{};
  window_class.cbSize = sizeof(window_class);
  window_class.lpfnWndProc = EventWindowProc;
  window_class.hInstance = GetModuleHandle(nullptr);
  window_class.lpszClassName = kEventWindowClass;
  RegisterClassExW(&window_class);  // Fails harmlessly if already registered.
  event_window_ = CreateWindowExW(0, kEventWindowClass, L"", 0, 0, 0, 0, 0,
                                  HWND_MESSAGE, nullptr, window_class.hInstance,
                                  this);

  media_foundation_started_ =
      SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_LITE));
}

MobileScannerPlugin::~MobileScannerPlugin() {
  StopCamera();
  if (event_window_) {
    DestroyWindow(event_window_);
    event_window_ = nullptr;
  }
  if (media_foundation_started_) {
    MFShutdown();
  }
}

LRESULT CALLBACK MobileScannerPlugin::EventWindowProc(HWND hwnd, UINT message,
                                                      WPARAM wparam,
                                                      LPARAM lparam) {
  if (message == WM_NCCREATE) {
    auto* create = reinterpret_cast<CREATESTRUCT*>(lparam);
    SetWindowLongPtr(hwnd, GWLP_USERDATA,
                     reinterpret_cast<LONG_PTR>(create->lpCreateParams));
  } else if (message == kDispatchMessage) {
    auto* self = reinterpret_cast<MobileScannerPlugin*>(
        GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (self) self->DrainPendingEvents();
    return 0;
  }
  return DefWindowProc(hwnd, message, wparam, lparam);
}

void MobileScannerPlugin::HandleMethodCall(
    const flutter::MethodCall<EncodableValue>& call,
    std::unique_ptr<Result> result) {
  const std::string& method = call.method_name();
  const auto* args = std::get_if<EncodableMap>(call.arguments());

  if (method == "state") {
    result->Success(EncodableValue(1));
  } else if (method == "request") {
    result->Success(EncodableValue(true));
  } else if (method == "start") {
    uint64_t format_mask = 0;
    if (args) {
      if (const auto* formats = FindKey(*args, "formats")) {
        if (const auto* list = std::get_if<EncodableList>(formats)) {
          for (const auto& format : *list) {
            uint64_t bits = 0;
            if (ReadInteger(format, &bits)) format_mask |= bits;
          }
        }
      }
    }
    EncodableMap response;
    std::string error;
    if (StartCamera(format_mask, &response, &error)) {
      result->Success(EncodableValue(response));
    } else {
      result->Error("camera_unavailable", error);
    }
  } else if (method == "stop" || method == "pause") {
    StopCamera();
    result->Success();
  } else if (method == "getSupportedLenses") {
    result->Success(EncodableValue(EncodableList()));
  } else if (method == "getBestCloseRangeScanningLens") {
    result->Success(EncodableValue(0));
  } else if (method == "setScale") {
    double zoom = 0.0;
    if (const auto* value = std::get_if<double>(call.arguments())) zoom = *value;
    SetZoom(zoom);
    result->Success();
  } else if (method == "resetScale") {
    SetZoom(0.0);
    result->Success();
  } else if (method == "toggleTorch") {
    // Media Foundation exposes no torch control, and webcams rarely have one.
    result->Error("unsupported", "This camera has no torch control.");
  } else if (method == "setFocus") {
    // Media Foundation exposes no focus control, so the position is ignored.
    result->Success();
  } else if (method == "updateScanWindow") {
    bool has_window = false;
    double window[4] = {0, 0, 1, 1};
    const EncodableValue* rect = args ? FindKey(*args, "rect") : nullptr;
    const auto* list = rect ? std::get_if<EncodableList>(rect) : nullptr;
    if (list && list->size() == 4) {
      has_window = true;
      for (size_t i = 0; i < 4; ++i) {
        const EncodableValue& value = (*list)[i];
        if (const auto* d = std::get_if<double>(&value)) {
          window[i] = *d;
        } else if (const auto* n32 = std::get_if<int32_t>(&value)) {
          window[i] = *n32;
        } else if (const auto* n64 = std::get_if<int64_t>(&value)) {
          window[i] = static_cast<double>(*n64);
        }
      }
    }
    {
      std::lock_guard<std::mutex> lock(window_mutex_);
      has_window_ = has_window;
      std::copy(window, window + 4, window_);
    }
    result->Success();
  } else {
    result->NotImplemented();
  }
}

bool MobileScannerPlugin::StartCamera(uint64_t format_mask,
                                      EncodableMap* response,
                                      std::string* error) {
  StopCamera();
  if (!media_foundation_started_) {
    *error = "Media Foundation could not be initialized";
    return false;
  }

  ComPtr<IMFAttributes> device_attributes;
  if (FAILED(MFCreateAttributes(&device_attributes, 1)) ||
      FAILED(device_attributes->SetGUID(
          MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
          MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID))) {
    *error = "Could not query video capture devices";
    return false;
  }

  IMFActivate** devices = nullptr;
  UINT32 device_count = 0;
  if (FAILED(MFEnumDeviceSources(device_attributes.Get(), &devices,
                                 &device_count)) ||
      device_count == 0) {
    if (devices) CoTaskMemFree(devices);
    *error = "No camera found";
    return false;
  }

  ComPtr<IMFMediaSource> source;
  HRESULT hr = devices[0]->ActivateObject(IID_PPV_ARGS(&source));
  for (UINT32 i = 0; i < device_count; ++i) devices[i]->Release();
  CoTaskMemFree(devices);
  if (FAILED(hr)) {
    *error = "Could not open the default camera";
    return false;
  }

  // Let Media Foundation convert webcam formats (MJPG, YUY2, NV12) to RGB32.
  ComPtr<IMFAttributes> reader_attributes;
  MFCreateAttributes(&reader_attributes, 1);
  reader_attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);

  ComPtr<IMFSourceReader> reader;
  hr = MFCreateSourceReaderFromMediaSource(source.Get(),
                                           reader_attributes.Get(), &reader);
  if (FAILED(hr)) {
    *error = "Could not create a camera reader";
    return false;
  }

  ComPtr<IMFMediaType> output_type;
  MFCreateMediaType(&output_type);
  output_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  output_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
  hr = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                   nullptr, output_type.Get());
  if (FAILED(hr)) {
    *error = "Camera does not support RGB32 frames";
    return false;
  }

  ComPtr<IMFMediaType> current_type;
  if (FAILED(reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                         &current_type))) {
    *error = "Could not read the camera frame format";
    return false;
  }
  UINT32 width = 0;
  UINT32 height = 0;
  MFGetAttributeSize(current_type.Get(), MF_MT_FRAME_SIZE, &width, &height);
  UINT32 stride = static_cast<UINT32>(width * 4);
  current_type->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride);
  if (width == 0 || height == 0) {
    *error = "Camera reported an empty frame size";
    return false;
  }

  frame_width_ = width;
  frame_height_ = height;
  stride_ = static_cast<int32_t>(stride);
  format_mask_ = format_mask;
  frames_seen_ = 0;
  last_decode_ = {};
  source_ = source;

  texture_ = std::make_unique<flutter::TextureVariant>(
      flutter::PixelBufferTexture([this](size_t w, size_t h) {
        return FillPixelBuffer(w, h);
      }));
  texture_id_ = textures_->RegisterTexture(texture_.get());

  running_ = true;
  worker_ = std::thread([this, reader]() mutable { CaptureLoop(reader); });

  (*response)[Key("textureId")] = EncodableValue(texture_id_);
  (*response)[Key("cameraDirection")] = EncodableValue(1);
  // Media Foundation has no torch or focus control, so only the digital zoom
  // is offered. A missing torch is reported as "unavailable" (-1).
  (*response)[Key("currentTorchState")] = EncodableValue(-1);
  (*response)[Key("numberOfCameras")] = EncodableValue(1);
  EncodableMap size;
  size[Key("width")] = EncodableValue(static_cast<double>(width));
  size[Key("height")] = EncodableValue(static_cast<double>(height));
  (*response)[Key("size")] = EncodableValue(size);
  EncodableMap capabilities;
  capabilities[Key("torch")] = EncodableValue(false);
  capabilities[Key("zoom")] = EncodableValue(true);
  capabilities[Key("focus")] = EncodableValue(false);
  (*response)[Key("capabilities")] = EncodableValue(capabilities);
  return true;
}

void MobileScannerPlugin::StopCamera() {
  running_ = false;

  // Shutting the source down makes a blocked ReadSample return promptly.
  if (source_) {
    ComPtr<IMFShutdown> shutdown;
    if (SUCCEEDED(source_.As(&shutdown))) shutdown->Shutdown();
  }
  if (worker_.joinable()) worker_.join();
  source_.Reset();

  if (texture_id_ != -1) {
    textures_->UnregisterTexture(texture_id_);
    texture_id_ = -1;
  }
  texture_.reset();

  {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    frame_.clear();
    texture_frame_.clear();
    latest_width_ = 0;
    latest_height_ = 0;
  }
  {
    std::lock_guard<std::mutex> lock(event_mutex_);
    pending_events_.clear();
  }
}

void MobileScannerPlugin::CaptureLoop(ComPtr<IMFSourceReader> reader) {
  CoInitializeEx(nullptr, COINIT_MULTITHREADED);

  while (running_) {
    DWORD stream_index = 0;
    DWORD flags = 0;
    LONGLONG timestamp = 0;
    ComPtr<IMFSample> sample;
    HRESULT hr = reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0,
                                    &stream_index, &flags, &timestamp, &sample);
    if (FAILED(hr) || (flags & (MF_SOURCE_READERF_ERROR |
                                MF_SOURCE_READERF_ENDOFSTREAM))) {
      break;
    }
    if (!sample) continue;

    ComPtr<IMFMediaBuffer> buffer;
    if (FAILED(sample->ConvertToContiguousBuffer(&buffer))) continue;

    BYTE* data = nullptr;
    DWORD max_length = 0;
    DWORD current_length = 0;
    if (FAILED(buffer->Lock(&data, &max_length, &current_length))) continue;
    ProcessFrame(data, current_length);
    buffer->Unlock();
  }

  reader.Reset();
  CoUninitialize();
}

void MobileScannerPlugin::ProcessFrame(const uint8_t* src, DWORD src_length) {
  // The first frames of a webcam are often corrupt or half-exposed.
  if (++frames_seen_ <= kWarmupFrames) return;

  const size_t row_bytes = static_cast<size_t>(frame_width_) * 4;
  const size_t abs_stride = static_cast<size_t>(std::abs(stride_));
  const bool bottom_up = stride_ < 0;
  if (abs_stride < row_bytes ||
      src_length < abs_stride * static_cast<size_t>(frame_height_)) {
    return;
  }

  // Copy into a top-down BGRA buffer. Negative stride means the image is
  // stored bottom-up, so the first memory row is the bottom image row.
  std::vector<uint8_t> frame(row_bytes * frame_height_);
  for (uint32_t y = 0; y < frame_height_; ++y) {
    const uint32_t source_row = bottom_up ? frame_height_ - 1 - y : y;
    std::memcpy(frame.data() + y * row_bytes,
                src + source_row * abs_stride, row_bytes);
  }

  // Digital zoom: keep only the centred crop. The texture and the decoder
  // both see this view.
  const ms_frame::Rect zoom_rect = ms_frame::ZoomRect(
      frame_width_, frame_height_, zoom_.load());
  const bool zoomed = zoom_rect.width != frame_width_ ||
                      zoom_rect.height != frame_height_;
  std::vector<uint8_t> view =
      zoomed ? ms_frame::CopyRect(frame.data(), frame_width_, zoom_rect)
             : std::move(frame);
  const uint32_t view_width = zoom_rect.width;
  const uint32_t view_height = zoom_rect.height;

  {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    frame_ = view;
    latest_width_ = view_width;
    latest_height_ = view_height;
  }
  if (texture_id_ != -1) {
    textures_->MarkTextureFrameAvailable(texture_id_);
  }

  const auto now = std::chrono::steady_clock::now();
  if (now - last_decode_ >= kDecodeInterval) {
    last_decode_ = now;
    DecodeFrame(view, view_width, view_height);
  }
}

void MobileScannerPlugin::DecodeFrame(const std::vector<uint8_t>& frame,
                                      uint32_t width, uint32_t height) {
  MsZxingDecodeParams params{};
  params.data = frame.data();
  params.width = static_cast<int32_t>(width);
  params.height = static_cast<int32_t>(height);
  params.row_stride = static_cast<int32_t>(width * 4);
  params.pixel_format = MS_ZXING_PIXEL_BGRA;
  params.format_mask = format_mask_;
  params.try_harder = 0;
  params.max_symbols = 10;

  // Scan only inside the scan window, if one is set.
  double window[4] = {0, 0, 1, 1};
  bool has_window = false;
  {
    std::lock_guard<std::mutex> lock(window_mutex_);
    has_window = has_window_;
    std::copy(window_, window_ + 4, window);
  }
  int32_t crop[4];
  ms_frame::ScanWindowCrop(has_window ? window : nullptr, width, height, crop);
  params.crop_left = crop[0];
  params.crop_top = crop[1];
  params.crop_width = crop[2];
  params.crop_height = crop[3];

  MsZxingResultList* result =
      ms_zxing_decode_sized_v2(&params, frame.size());
  if (result && result->count > 0) {
    EncodableList barcodes;
    for (int32_t i = 0; i < result->count; ++i) {
      const MsZxingResult& barcode = result->results[i];
      EncodableMap item;
      item[Key("format")] =
          EncodableValue(static_cast<int64_t>(barcode.format));
      item[Key("rawValue")] =
          EncodableValue(barcode.text ? std::string(barcode.text) : "");
      if (barcode.bytes && barcode.bytes_length > 0) {
        item[Key("rawBytes")] = EncodableValue(
            std::vector<uint8_t>(barcode.bytes,
                                 barcode.bytes + barcode.bytes_length));
      }
      item[Key("type")] = EncodableValue(0);

      float left = barcode.corners[0].x, right = left;
      float top = barcode.corners[0].y, bottom = top;
      EncodableList corners;
      for (const auto& point : barcode.corners) {
        left = std::min(left, point.x);
        right = std::max(right, point.x);
        top = std::min(top, point.y);
        bottom = std::max(bottom, point.y);
        EncodableMap corner;
        corner[Key("x")] = EncodableValue(static_cast<double>(point.x));
        corner[Key("y")] = EncodableValue(static_cast<double>(point.y));
        corners.push_back(EncodableValue(corner));
      }
      EncodableMap size;
      size[Key("width")] = EncodableValue(static_cast<double>(right - left));
      size[Key("height")] = EncodableValue(static_cast<double>(bottom - top));
      item[Key("size")] = EncodableValue(size);
      item[Key("corners")] = EncodableValue(corners);
      barcodes.push_back(EncodableValue(item));
    }

    EncodableMap image;
    image[Key("width")] = EncodableValue(static_cast<double>(width));
    image[Key("height")] = EncodableValue(static_cast<double>(height));
    EncodableMap event;
    event[Key("name")] = Key("barcode");
    event[Key("data")] = EncodableValue(barcodes);
    event[Key("image")] = EncodableValue(image);
    PostBarcodeEvent(EncodableValue(event));
  }
  if (result) ms_zxing_free_results(result);
}

void MobileScannerPlugin::PostBarcodeEvent(EncodableValue event) {
  {
    std::lock_guard<std::mutex> lock(event_mutex_);
    pending_events_.push_back(std::move(event));
  }
  if (event_window_) PostMessage(event_window_, kDispatchMessage, 0, 0);
}

void MobileScannerPlugin::DrainPendingEvents() {
  std::deque<EncodableValue> events;
  {
    std::lock_guard<std::mutex> lock(event_mutex_);
    events.swap(pending_events_);
  }
  if (!event_sink_) return;
  for (const auto& event : events) event_sink_->Success(event);
}

void MobileScannerPlugin::SetZoom(double zoom) {
  zoom = std::clamp(zoom, 0.0, 1.0);
  zoom_ = zoom;
  SendEvent("zoomScaleState", EncodableValue(zoom));
}

// Runs on the platform thread only, because it calls the Flutter event sink.
void MobileScannerPlugin::SendEvent(const char* name, EncodableValue data) {
  if (!event_sink_) return;
  EncodableMap event;
  event[Key("name")] = Key(name);
  event[Key("data")] = std::move(data);
  event_sink_->Success(EncodableValue(event));
}

const FlutterDesktopPixelBuffer* MobileScannerPlugin::FillPixelBuffer(
    size_t /*width*/, size_t /*height*/) {
  std::lock_guard<std::mutex> lock(frame_mutex_);
  if (frame_.empty() || latest_width_ == 0 || latest_height_ == 0) {
    return nullptr;
  }
  texture_frame_ = frame_;
  texture_width_ = latest_width_;
  texture_height_ = latest_height_;
  pixel_buffer_.buffer = texture_frame_.data();
  pixel_buffer_.width = texture_width_;
  pixel_buffer_.height = texture_height_;
  pixel_buffer_.release_callback = nullptr;
  pixel_buffer_.release_context = nullptr;
  return &pixel_buffer_;
}

extern "C" FLUTTER_PLUGIN_EXPORT void MobileScannerPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  MobileScannerPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
