// Implementation of the shared C ABI declared in ms_zxing.h, backed by
// zxing-cpp (https://github.com/zxing-cpp/zxing-cpp).
//
// Build note: this translation unit is compiled into the plugin's native
// library on every platform (see src/CMakeLists.txt). zxing-cpp itself is
// pulled in by that CMake file via FetchContent, so no headers are vendored.

#include "ms_zxing.h"
#include "ms_unsupported_barcodes.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <ReadBarcode.h>
#include <BarcodeFormat.h>
#include <ImageView.h>
#include <Version.h>

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
constexpr uint32_t kRawCode11 = 131072;

struct Code11Candidate {
  std::string text;
  int32_t start = 0;
  int32_t end = 0;
  int32_t fixed = 0;
  bool vertical = false;
};

struct Run {
  int32_t start = 0;
  int32_t width = 0;
  bool black = false;
};

struct Code11Symbol {
  std::string_view pattern;
  char value;
};

constexpr Code11Symbol kCode11Symbols[] = {
    {"00001", '0'}, {"10001", '1'}, {"01001", '2'}, {"11000", '3'},
    {"00101", '4'}, {"10100", '5'}, {"01100", '6'}, {"00011", '7'},
    {"10010", '8'}, {"10000", '9'}, {"00100", '-'}, {"00110", '*'},
};

// Translate a Dart BarcodeFormat.rawValue bitmask into zxing-cpp's
// BarcodeFormats. An empty/zero mask means "all supported formats".
BarcodeFormats RawMaskToZxing(uint32_t mask) {
  if (mask == 0) {
    return BarcodeFormat::Any;
  }
  // BarcodeFormat.itf2of5 (126) and itf2of5WithChecksum (127) are NOT
  // power-of-two flags: 126 == code39|code93|codabar|dataMatrix|ean13|ean8 and
  // 127 adds code128. So they can only be interpreted unambiguously when they
  // arrive on their own (the common case of asking for just ITF). Map those
  // exact values straight to ITF. Once OR-ed with any other format the value is
  // indistinguishable from a combination of the real bit flags, so it falls
  // through to the bitwise handling below (zxing-cpp has a single ITF format).
  if (mask == 126u || mask == 127u) {
    return BarcodeFormat::ITF;
  }
  BarcodeFormats formats = BarcodeFormat::None;
  if (mask & kRawCode128) formats |= BarcodeFormat::Code128;
  if (mask & kRawCode39) formats |= BarcodeFormat::Code39;
  if (mask & kRawCode93) formats |= BarcodeFormat::Code93;
  if (mask & kRawCodabar) formats |= BarcodeFormat::Codabar;
  if (mask & kRawDataMatrix) formats |= BarcodeFormat::DataMatrix;
  if (mask & kRawEan13) formats |= BarcodeFormat::EAN13;
  if (mask & kRawEan8) formats |= BarcodeFormat::EAN8;
  // Dedicated ITF bit only (itf / itf14 == 128). Do NOT test 126/127 here:
  // those are handled above and their bits otherwise collide with the linear
  // formats, which previously made e.g. a plain EAN-13 request (mask 32) also
  // enable ITF and produce spurious reads.
  if (mask & kRawItf) formats |= BarcodeFormat::ITF;
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
  // If nothing matched (e.g. only unsupported formats requested), keep detection
  // disabled. Falling back to Any would violate the caller's format filter.
  if (formats == BarcodeFormat::None) {
    return BarcodeFormat::None;
  }
  return formats;
}

bool Code11Requested(uint32_t mask) {
  return mask == 0 || (mask & kRawCode11) != 0;
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
    default: return -1;  // unknown
  }
}

int32_t BytesPerPixel(int32_t pixel_format) {
  switch (pixel_format) {
    case MS_ZXING_PIXEL_RGB:
      return 3;
    case MS_ZXING_PIXEL_RGBA:
    case MS_ZXING_PIXEL_BGRA:
      return 4;
    case MS_ZXING_PIXEL_LUM:
    default:
      return 1;
  }
}

uint8_t LuminanceAt(const MsZxingDecodeParams& params, int32_t x, int32_t y) {
  const int32_t bpp = BytesPerPixel(params.pixel_format);
  const int32_t stride = params.row_stride > 0 ? params.row_stride
                                               : params.width * bpp;
  const uint8_t* pixel = params.data + y * stride + x * bpp;
  switch (params.pixel_format) {
    case MS_ZXING_PIXEL_RGB:
      return static_cast<uint8_t>((77 * pixel[0] + 150 * pixel[1] +
                                   29 * pixel[2]) >> 8);
    case MS_ZXING_PIXEL_RGBA:
      return static_cast<uint8_t>((77 * pixel[0] + 150 * pixel[1] +
                                   29 * pixel[2]) >> 8);
    case MS_ZXING_PIXEL_BGRA:
      return static_cast<uint8_t>((77 * pixel[2] + 150 * pixel[1] +
                                   29 * pixel[0]) >> 8);
    case MS_ZXING_PIXEL_LUM:
    default:
      return *pixel;
  }
}

std::optional<std::vector<Run>> ExtractRuns(const MsZxingDecodeParams& params,
                                            int32_t originX, int32_t originY,
                                            int32_t width, int32_t height,
                                            int32_t fixed, bool vertical) {
  const int32_t length = vertical ? height : width;
  if (length < 24) return std::nullopt;

  uint8_t minLum = 255;
  uint8_t maxLum = 0;
  std::vector<uint8_t> line;
  line.reserve(length);
  for (int32_t i = 0; i < length; ++i) {
    const int32_t x = vertical ? fixed : i;
    const int32_t y = vertical ? i : fixed;
    const uint8_t lum = LuminanceAt(params, originX + x, originY + y);
    line.push_back(lum);
    minLum = std::min(minLum, lum);
    maxLum = std::max(maxLum, lum);
  }

  if (maxLum - minLum < 32) return std::nullopt;

  const uint8_t threshold =
      static_cast<uint8_t>((static_cast<int32_t>(minLum) + maxLum) / 2);
  std::vector<Run> runs;
  bool currentBlack = line.front() < threshold;
  int32_t start = 0;
  for (int32_t i = 1; i < length; ++i) {
    const bool black = line[i] < threshold;
    if (black == currentBlack) continue;
    runs.push_back(Run{.start = start, .width = i - start, .black = currentBlack});
    start = i;
    currentBlack = black;
  }
  runs.push_back(
      Run{.start = start, .width = length - start, .black = currentBlack});

  while (!runs.empty() && !runs.front().black) {
    runs.erase(runs.begin());
  }
  while (!runs.empty() && !runs.back().black) {
    runs.pop_back();
  }
  if (runs.size() < 23) return std::nullopt;
  return runs;
}

std::optional<double> WideThreshold(const std::vector<Run>& runs,
                                    size_t start, size_t count) {
  int32_t minWidth = runs[start].width;
  int32_t maxWidth = runs[start].width;
  for (size_t i = start; i < start + count; ++i) {
    minWidth = std::min(minWidth, runs[i].width);
    maxWidth = std::max(maxWidth, runs[i].width);
  }
  if (minWidth <= 0 || maxWidth < minWidth * 1.45) return std::nullopt;

  double narrow = minWidth;
  double wide = maxWidth;
  for (int i = 0; i < 8; ++i) {
    double narrowSum = 0;
    double wideSum = 0;
    int narrowCount = 0;
    int wideCount = 0;
    const double threshold = (narrow + wide) / 2.0;
    for (size_t j = start; j < start + count; ++j) {
      if (runs[j].width <= threshold) {
        narrowSum += runs[j].width;
        ++narrowCount;
      } else {
        wideSum += runs[j].width;
        ++wideCount;
      }
    }
    if (narrowCount == 0 || wideCount == 0) return std::nullopt;
    narrow = narrowSum / narrowCount;
    wide = wideSum / wideCount;
  }

  if (wide / narrow < 1.45 || wide / narrow > 3.1) return std::nullopt;
  return (narrow + wide) / 2.0;
}

std::optional<char> DecodeCode11Pattern(std::string_view pattern) {
  for (const auto& symbol : kCode11Symbols) {
    if (symbol.pattern == pattern) return symbol.value;
  }
  return std::nullopt;
}

std::optional<std::string> DecodeCode11Slice(const std::vector<Run>& runs,
                                             size_t start, size_t count) {
  if (count < 23 || count % 6 != 5 || !runs[start].black ||
      !runs[start + count - 1].black) {
    return std::nullopt;
  }

  const auto threshold = WideThreshold(runs, start, count);
  if (!threshold.has_value()) return std::nullopt;

  std::string decoded;
  const size_t symbolCount = (count + 1) / 6;
  decoded.reserve(symbolCount);
  for (size_t symbol = 0; symbol < symbolCount; ++symbol) {
    const size_t offset = start + symbol * 6;
    std::string pattern;
    pattern.reserve(5);
    for (size_t i = 0; i < 5; ++i) {
      const Run& run = runs[offset + i];
      const bool expectedBlack = i % 2 == 0;
      if (run.black != expectedBlack) return std::nullopt;
      pattern.push_back(run.width > *threshold ? '1' : '0');
    }
    const auto value = DecodeCode11Pattern(pattern);
    if (!value.has_value()) return std::nullopt;
    decoded.push_back(*value);

    if (symbol + 1 < symbolCount) {
      const Run& separator = runs[offset + 5];
      if (separator.black || separator.width > *threshold) return std::nullopt;
    }
  }

  if (decoded.size() < 4 || decoded.front() != '*' || decoded.back() != '*') {
    return std::nullopt;
  }

  const std::string withChecks = decoded.substr(1, decoded.size() - 2);
  const auto validated =
      mobile_scanner::unsupported::ValidateCode11(withChecks);
  if (!validated.has_value() || !validated->checksumValid) return std::nullopt;
  return validated->text;
}

std::optional<Code11Candidate> DecodeCode11Runs(const std::vector<Run>& runs,
                                                int32_t fixed,
                                                bool vertical) {
  std::vector<size_t> starts;
  for (size_t i = 0; i < runs.size(); ++i) {
    if (runs[i].black) starts.push_back(i);
  }

  for (const size_t start : starts) {
    for (size_t count = 23; start + count <= runs.size(); count += 6) {
      auto text = DecodeCode11Slice(runs, start, count);
      if (!text.has_value()) {
        std::vector<Run> reversed(runs.begin() + start,
                                  runs.begin() + start + count);
        std::reverse(reversed.begin(), reversed.end());
        int32_t cursor = 0;
        for (auto& run : reversed) {
          run.start = cursor;
          cursor += run.width;
        }
        text = DecodeCode11Slice(reversed, 0, reversed.size());
      }
      if (!text.has_value()) continue;

      const int32_t startCoord = runs[start].start;
      const Run& last = runs[start + count - 1];
      return Code11Candidate{
          .text = *text,
          .start = startCoord,
          .end = last.start + last.width,
          .fixed = fixed,
          .vertical = vertical,
      };
    }
  }
  return std::nullopt;
}

std::vector<Code11Candidate> DecodeCode11(const MsZxingDecodeParams& params) {
  const int32_t originX =
      params.crop_width > 0 && params.crop_height > 0 ? params.crop_left : 0;
  const int32_t originY =
      params.crop_width > 0 && params.crop_height > 0 ? params.crop_top : 0;
  const int32_t width =
      params.crop_width > 0 && params.crop_height > 0 ? params.crop_width
                                                      : params.width;
  const int32_t height =
      params.crop_width > 0 && params.crop_height > 0 ? params.crop_height
                                                      : params.height;

  std::vector<Code11Candidate> candidates;
  const int32_t horizontalSamples = params.try_harder != 0 ? 25 : 9;
  for (int32_t sample = 0; sample < horizontalSamples; ++sample) {
    const int32_t y =
        ((sample + 1) * height) / (horizontalSamples + 1);
    const auto runs = ExtractRuns(params, originX, originY, width, height, y,
                                  false);
    if (!runs.has_value()) continue;
    auto candidate = DecodeCode11Runs(*runs, originY + y, false);
    if (!candidate.has_value()) continue;
    candidate->start += originX;
    candidate->end += originX;
    candidates.push_back(*candidate);
    break;
  }

  if (params.try_rotate != 0) {
    const int32_t verticalSamples = params.try_harder != 0 ? 25 : 9;
    for (int32_t sample = 0; sample < verticalSamples; ++sample) {
      const int32_t x =
          ((sample + 1) * width) / (verticalSamples + 1);
      const auto runs = ExtractRuns(params, originX, originY, width, height, x,
                                    true);
      if (!runs.has_value()) continue;
      auto candidate = DecodeCode11Runs(*runs, originX + x, true);
      if (!candidate.has_value()) continue;
      candidate->start += originY;
      candidate->end += originY;
      candidates.push_back(*candidate);
      break;
    }
  }

  return candidates;
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
    std::vector<Code11Candidate> code11Candidates;
    if (Code11Requested(params->format_mask)) {
      code11Candidates = DecodeCode11(*params);
    }
    const size_t resultCapacity = barcodes.size() + code11Candidates.size();
    if (resultCapacity == 0) {
      return list;
    }

    auto* results = static_cast<MsZxingResult*>(
        std::calloc(resultCapacity, sizeof(MsZxingResult)));
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

    for (const auto& candidate : code11Candidates) {
      if (i >= static_cast<int32_t>(resultCapacity)) break;
      MsZxingResult& r = results[i];
      r.format = kRawCode11;
      r.text = DupString(candidate.text);

      if (!candidate.text.empty()) {
        auto* buf = static_cast<uint8_t*>(std::malloc(candidate.text.size()));
        if (buf != nullptr) {
          std::memcpy(buf, candidate.text.data(), candidate.text.size());
          r.bytes = buf;
          r.bytes_length = static_cast<int32_t>(candidate.text.size());
        }
      }

      if (candidate.vertical) {
        const float x = static_cast<float>(candidate.fixed);
        const float top = static_cast<float>(candidate.start);
        const float bottom = static_cast<float>(candidate.end);
        r.corners[0] = MsZxingPoint{.x = x - 1.0f, .y = top};
        r.corners[1] = MsZxingPoint{.x = x + 1.0f, .y = top};
        r.corners[2] = MsZxingPoint{.x = x + 1.0f, .y = bottom};
        r.corners[3] = MsZxingPoint{.x = x - 1.0f, .y = bottom};
      } else {
        const float left = static_cast<float>(candidate.start);
        const float right = static_cast<float>(candidate.end);
        const float y = static_cast<float>(candidate.fixed);
        r.corners[0] = MsZxingPoint{.x = left, .y = y - 1.0f};
        r.corners[1] = MsZxingPoint{.x = right, .y = y - 1.0f};
        r.corners[2] = MsZxingPoint{.x = right, .y = y + 1.0f};
        r.corners[3] = MsZxingPoint{.x = left, .y = y + 1.0f};
      }

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
