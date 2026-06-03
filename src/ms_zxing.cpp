// Implementation of the shared C ABI declared in ms_zxing.h, backed by
// zxing-cpp (https://github.com/zxing-cpp/zxing-cpp).
//
// Build note: this translation unit is compiled into the plugin's native
// library on every platform (see src/CMakeLists.txt). zxing-cpp itself is
// pulled in by that CMake file via FetchContent, so no headers are vendored.

#include "ms_zxing.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <ZXing/ReadBarcode.h>
#include <ZXing/BarcodeFormat.h>
#include <ZXing/ImageView.h>
#include <ZXing/Version.h>

using namespace ZXing;

namespace {

// --- Format mapping -------------------------------------------------------
// Keep these in sync with lib/src/enums/barcode_format.dart.
constexpr uint32_t kRawCode128 = 1;
constexpr uint32_t kRawCode39 = 2;
constexpr uint32_t kRawCode93 = 4;
constexpr uint32_t kRawCodabar = 8;
constexpr uint32_t kRawDataMatrix = 16;
constexpr uint32_t kRawEan13 = 32;
constexpr uint32_t kRawEan8 = 64;
constexpr uint32_t kRawItf = 128;  // covers itf2of5/itf2of5WithChecksum/itf14
constexpr uint32_t kRawQr = 256;
constexpr uint32_t kRawUpcA = 512;
constexpr uint32_t kRawUpcE = 1024;
constexpr uint32_t kRawPdf417 = 2048;
constexpr uint32_t kRawAztec = 4096;
constexpr uint32_t kRawDataBar = 8192;
constexpr uint32_t kRawDataBarExpanded = 16384;
constexpr uint32_t kRawMaxiCode = 32768;
constexpr uint32_t kRawDotCode = 65536;

// Translate a Dart BarcodeFormat.rawValue bitmask into zxing-cpp's
// BarcodeFormats. An empty/zero mask means "all supported formats".
BarcodeFormats RawMaskToZxing(uint32_t mask) {
  if (mask == 0) {
    return BarcodeFormat::Any;
  }
  BarcodeFormats formats = BarcodeFormat::None;
  if (mask & kRawCode128) formats |= BarcodeFormat::Code128;
  if (mask & kRawCode39) formats |= BarcodeFormat::Code39;
  if (mask & kRawCode93) formats |= BarcodeFormat::Code93;
  if (mask & kRawCodabar) formats |= BarcodeFormat::Codabar;
  if (mask & kRawDataMatrix) formats |= BarcodeFormat::DataMatrix;
  if (mask & kRawEan13) formats |= BarcodeFormat::EAN13;
  if (mask & kRawEan8) formats |= BarcodeFormat::EAN8;
  // The Dart enum exposes several ITF variants; the lower itf bits (126/127)
  // also fold onto the 128 bit when serialized, so treat any ITF bit as ITF.
  if (mask & kRawItf || mask & 126u || mask & 127u) formats |= BarcodeFormat::ITF;
  if (mask & kRawQr) formats |= BarcodeFormat::QRCode;
  if (mask & kRawUpcA) formats |= BarcodeFormat::UPCA;
  if (mask & kRawUpcE) formats |= BarcodeFormat::UPCE;
  if (mask & kRawPdf417) formats |= BarcodeFormat::PDF417;
  if (mask & kRawAztec) formats |= BarcodeFormat::Aztec;
  if (mask & kRawDataBar) formats |= BarcodeFormat::DataBar;
  if (mask & kRawDataBarExpanded) formats |= BarcodeFormat::DataBarExpanded;
  if (mask & kRawMaxiCode) formats |= BarcodeFormat::MaxiCode;
#ifdef ZXING_HAS_DOTCODE
  if (mask & kRawDotCode) formats |= BarcodeFormat::DotCode;
#endif
  // If nothing matched (e.g. only unsupported formats requested), fall back to
  // Any so we don't silently disable detection.
  if (formats == BarcodeFormat::None) {
    return BarcodeFormat::Any;
  }
  return formats;
}

// Translate a zxing-cpp BarcodeFormat back to a Dart rawValue.
int32_t ZxingToRaw(BarcodeFormat format) {
  switch (format) {
    case BarcodeFormat::Code128: return kRawCode128;
    case BarcodeFormat::Code39: return kRawCode39;
    case BarcodeFormat::Code93: return kRawCode93;
    case BarcodeFormat::Codabar: return kRawCodabar;
    case BarcodeFormat::DataMatrix: return kRawDataMatrix;
    case BarcodeFormat::EAN13: return kRawEan13;
    case BarcodeFormat::EAN8: return kRawEan8;
    case BarcodeFormat::ITF: return kRawItf;
    case BarcodeFormat::QRCode: return kRawQr;
    case BarcodeFormat::UPCA: return kRawUpcA;
    case BarcodeFormat::UPCE: return kRawUpcE;
    case BarcodeFormat::PDF417: return kRawPdf417;
    case BarcodeFormat::Aztec: return kRawAztec;
    case BarcodeFormat::DataBar: return kRawDataBar;
    case BarcodeFormat::DataBarExpanded: return kRawDataBarExpanded;
    case BarcodeFormat::MaxiCode: return kRawMaxiCode;
#ifdef ZXING_HAS_DOTCODE
    case BarcodeFormat::DotCode: return kRawDotCode;
#endif
    default: return 0;  // unknown
  }
}

ImageFormat PixelToImageFormat(int32_t pixel_format) {
  switch (pixel_format) {
    case MS_ZXING_PIXEL_RGB: return ImageFormat::RGB;
    case MS_ZXING_PIXEL_RGBA: return ImageFormat::RGBA;
    case MS_ZXING_PIXEL_BGRA: return ImageFormat::BGRA;
    case MS_ZXING_PIXEL_LUM:
    default:
      return ImageFormat::Lum;
  }
}

char* DupString(const std::string& s) {
  char* out = static_cast<char*>(std::malloc(s.size() + 1));
  if (out == nullptr) return nullptr;
  std::memcpy(out, s.data(), s.size());
  out[s.size()] = '\0';
  return out;
}

}  // namespace

extern "C" {

MsZxingResultList* ms_zxing_decode(const MsZxingDecodeParams* params) {
  auto* list = static_cast<MsZxingResultList*>(
      std::calloc(1, sizeof(MsZxingResultList)));
  if (list == nullptr) return nullptr;
  list->results = nullptr;
  list->count = 0;

  if (params == nullptr || params->data == nullptr || params->width <= 0 ||
      params->height <= 0) {
    return list;  // empty list
  }

  try {
    const ImageFormat imageFormat = PixelToImageFormat(params->pixel_format);
    ImageView image(params->data, params->width, params->height, imageFormat,
                    params->row_stride);

    // Restrict scanning to the requested crop (scan window) when provided.
    // Corner coordinates are offset back to full-image space below.
    float cropOffsetX = 0.0f;
    float cropOffsetY = 0.0f;
    if (params->crop_width > 0 && params->crop_height > 0) {
      image = image.cropped(params->crop_left, params->crop_top,
                            params->crop_width, params->crop_height);
      cropOffsetX = static_cast<float>(params->crop_left);
      cropOffsetY = static_cast<float>(params->crop_top);
    }

    ReaderOptions options;
    options.setFormats(RawMaskToZxing(params->format_mask));
    options.setTryHarder(params->try_harder != 0);
    options.setTryRotate(params->try_rotate != 0);
    options.setTryInvert(params->try_invert != 0);
    options.setTryDownscale(params->try_downscale != 0);
    options.setMaxNumberOfSymbols(
        params->max_symbols > 0 ? params->max_symbols : 16);

    const Barcodes barcodes = ReadBarcodes(image, options);
    if (barcodes.empty()) {
      return list;
    }

    auto* results = static_cast<MsZxingResult*>(
        std::calloc(barcodes.size(), sizeof(MsZxingResult)));
    if (results == nullptr) {
      return list;  // OOM: return empty rather than crash
    }

    int32_t i = 0;
    for (const auto& barcode : barcodes) {
      if (!barcode.isValid()) continue;

      MsZxingResult& r = results[i];
      r.format = ZxingToRaw(barcode.format());

      const std::string text = barcode.text();
      r.text = DupString(text);

      const ByteArray& bytes = barcode.bytes();
      if (!bytes.empty()) {
        auto* buf = static_cast<uint8_t*>(std::malloc(bytes.size()));
        if (buf != nullptr) {
          std::memcpy(buf, bytes.data(), bytes.size());
          r.bytes = buf;
          r.bytes_length = static_cast<int32_t>(bytes.size());
        }
      }

      const Position pos = barcode.position();
      const auto assign = [cropOffsetX, cropOffsetY](MsZxingPoint& p,
                                                     const PointI& src) {
        p.x = static_cast<float>(src.x) + cropOffsetX;
        p.y = static_cast<float>(src.y) + cropOffsetY;
      };
      assign(r.corners[0], pos.topLeft());
      assign(r.corners[1], pos.topRight());
      assign(r.corners[2], pos.bottomRight());
      assign(r.corners[3], pos.bottomLeft());

      ++i;
    }

    list->results = results;
    list->count = i;
  } catch (...) {
    // Never let an exception cross the C ABI boundary; return what we have.
  }

  return list;
}

void ms_zxing_free_results(MsZxingResultList* list) {
  if (list == nullptr) return;
  if (list->results != nullptr) {
    for (int32_t i = 0; i < list->count; ++i) {
      std::free(const_cast<char*>(list->results[i].text));
      std::free(const_cast<uint8_t*>(list->results[i].bytes));
    }
    std::free(list->results);
  }
  std::free(list);
}

const char* ms_zxing_version(void) {
  return ZXING_VERSION_STR;
}

}  // extern "C"
