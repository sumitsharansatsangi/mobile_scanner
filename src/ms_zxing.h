// Shared C ABI over zxing-cpp for the mobile_scanner plugin.
//
// This header is the single decode entry point used by every platform:
//   - Android: called from Kotlin via JNI (camera hot path).
//   - iOS/macOS: called from Swift via the C interop (camera hot path).
//   - Linux/Windows/all: called from Dart via dart:ffi (analyzeImage + desktop).
//
// The format integers exchanged here intentionally match the `rawValue`s of
// the Dart `BarcodeFormat` enum (see lib/src/enums/barcode_format.dart) so the
// same bitmask can travel end-to-end without translation tables in Dart.
//
// Design follows the proven flutter_zxing C ABI (params-struct + result list),
// adapted to mobile_scanner's format ids and reader-only scope.

#ifndef MS_ZXING_H
#define MS_ZXING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Visibility / calling-convention macro so the symbols are exported from the
// shared library on every toolchain.
#if defined(_WIN32)
#define MS_ZXING_API __declspec(dllexport)
#else
#define MS_ZXING_API __attribute__((visibility("default"))) __attribute__((used))
#endif

// Layout of the pixel buffer passed to ms_zxing_decode.
typedef enum {
  MS_ZXING_PIXEL_LUM = 0,   // 8-bit grayscale, 1 byte/pixel (preferred fast path)
  MS_ZXING_PIXEL_RGB = 1,   // 24-bit RGB
  MS_ZXING_PIXEL_RGBA = 2,  // 32-bit RGBA
  MS_ZXING_PIXEL_BGRA = 3,  // 32-bit BGRA (e.g. Apple Vision / 32BGRA buffers)
} MsZxingPixelFormat;

// Decode request. Fields left at 0 take their documented default, so callers
// only need to set what they care about.
typedef struct {
  const uint8_t* data;   // pointer to the first pixel (required)
  int32_t width;         // image width in pixels (required)
  int32_t height;        // image height in pixels (required)
  int32_t row_stride;    // bytes per row; 0 = tightly packed
  int32_t pixel_format;  // one of MsZxingPixelFormat

  // OR-ed BarcodeFormat.rawValue bits to search for; 0 = every broadly safe
  // supported format. Checksum-less fallback formats such as Pharmacode require
  // an explicit format bit to avoid false positives.
  uint32_t format_mask;

  // Optional crop rectangle in pixels, used to restrict scanning to a scan
  // window. Leave all four at 0 to scan the whole image.
  int32_t crop_left;
  int32_t crop_top;
  int32_t crop_width;
  int32_t crop_height;

  // zxing-cpp ReaderOptions toggles. Use 0 for the per-frame fast path and 1
  // for retries / analyzeImage where robustness matters more than latency.
  int32_t try_harder;
  int32_t try_rotate;
  int32_t try_invert;
  int32_t try_downscale;

  // Maximum number of barcodes to return; 0 = engine default.
  int32_t max_symbols;
} MsZxingDecodeParams;

// A corner point in image pixel coordinates.
typedef struct {
  float x;
  float y;
} MsZxingPoint;

// One decoded barcode.
typedef struct {
  // BarcodeFormat.rawValue (matches the Dart enum). -1 if unknown.
  int32_t format;
  // UTF-8 decoded text, NUL-terminated. Owned by the result list.
  const char* text;
  // Raw decoded bytes (may contain NULs). Owned by the result list.
  const uint8_t* bytes;
  int32_t bytes_length;
  // Corner points: [0]=topLeft, [1]=topRight, [2]=bottomRight, [3]=bottomLeft,
  // already mapped back to full-image coordinates when a crop was used.
  MsZxingPoint corners[4];
} MsZxingResult;

// A heap-allocated list of results returned by ms_zxing_decode.
typedef struct {
  MsZxingResult* results;
  int32_t count;
} MsZxingResultList;

// Decode barcodes from a single image buffer described by [params].
//
// Always returns a non-null, heap-allocated list (count may be 0). The caller
// MUST free it with ms_zxing_free_results. Returned strings/bytes are owned by
// the list and are invalidated once it is freed.
MS_ZXING_API MsZxingResultList* ms_zxing_decode(
    const MsZxingDecodeParams* params);

// Free a list returned by ms_zxing_decode (null-safe).
MS_ZXING_API void ms_zxing_free_results(MsZxingResultList* list);

// Returns the linked zxing-cpp version string (for diagnostics / about pages).
MS_ZXING_API const char* ms_zxing_version(void);

#ifdef __cplusplus
}
#endif

#endif  // MS_ZXING_H
