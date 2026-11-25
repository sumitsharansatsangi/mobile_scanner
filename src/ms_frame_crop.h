// Frame helpers shared by the desktop live-camera plugins (Linux, Windows).
//
// Works on tightly packed 32-bit pixel buffers (RGBA or BGRA). Header-only so
// it needs no extra build target.

#ifndef MS_FRAME_CROP_H
#define MS_FRAME_CROP_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace ms_frame {

// Largest digital zoom factor applied when the camera has no optical zoom.
constexpr double kMaxDigitalZoom = 4.0;

struct Rect {
  uint32_t left;
  uint32_t top;
  uint32_t width;
  uint32_t height;
};

// Maps a linear zoom in [0, 1] to a centered crop of a width x height frame.
// zoom <= 0 returns the whole frame.
inline Rect ZoomRect(uint32_t width, uint32_t height, double zoom) {
  zoom = std::clamp(zoom, 0.0, 1.0);
  const double ratio = 1.0 + zoom * (kMaxDigitalZoom - 1.0);
  uint32_t w = std::max<uint32_t>(static_cast<uint32_t>(width / ratio), 1);
  uint32_t h = std::max<uint32_t>(static_cast<uint32_t>(height / ratio), 1);
  return Rect{(width - w) / 2, (height - h) / 2, w, h};
}

// Copies a rectangle out of a packed 32-bit frame that is src_width pixels wide.
inline std::vector<uint8_t> CopyRect(const uint8_t* src, uint32_t src_width,
                                     Rect rect) {
  std::vector<uint8_t> out(static_cast<size_t>(rect.width) * rect.height * 4);
  const size_t src_row = static_cast<size_t>(src_width) * 4;
  const size_t dst_row = static_cast<size_t>(rect.width) * 4;
  for (uint32_t y = 0; y < rect.height; ++y) {
    std::memcpy(out.data() + y * dst_row,
                src + (rect.top + y) * src_row + static_cast<size_t>(rect.left) * 4,
                dst_row);
  }
  return out;
}

// Converts a normalized scan window into ZXing crop parameters.
//
// window points at [left, top, right, bottom] in the range 0..1 of the frame,
// or is null for the whole frame. On return crop holds crop_left, crop_top,
// crop_width and crop_height in pixels, all zero meaning "whole image".
inline void ScanWindowCrop(const double* window, uint32_t width, uint32_t height,
                           int32_t crop[4]) {
  crop[0] = crop[1] = crop[2] = crop[3] = 0;
  if (window == nullptr || width == 0 || height == 0) return;

  const double w = static_cast<double>(width);
  const double h = static_cast<double>(height);
  const double left = std::clamp(window[0], 0.0, 1.0);
  const double top = std::clamp(window[1], 0.0, 1.0);
  const double right = std::clamp(window[2], 0.0, 1.0);
  const double bottom = std::clamp(window[3], 0.0, 1.0);

  const uint32_t x0 = static_cast<uint32_t>(std::floor(left * w));
  const uint32_t y0 = static_cast<uint32_t>(std::floor(top * h));
  const uint32_t x1 = std::min<uint32_t>(static_cast<uint32_t>(std::ceil(right * w)), width);
  const uint32_t y1 = std::min<uint32_t>(static_cast<uint32_t>(std::ceil(bottom * h)), height);

  // A degenerate window falls back to the whole frame rather than scanning nothing.
  if (x1 <= x0 || y1 <= y0) return;
  if (x0 == 0 && y0 == 0 && x1 == width && y1 == height) return;

  crop[0] = static_cast<int32_t>(x0);
  crop[1] = static_cast<int32_t>(y0);
  crop[2] = static_cast<int32_t>(x1 - x0);
  crop[3] = static_cast<int32_t>(y1 - y0);
}

}  // namespace ms_frame

#endif  // MS_FRAME_CROP_H
