// Implementation of the shared C ABI declared in ms_zxing.h, backed by
// zxing-cpp (https://github.com/zxing-cpp/zxing-cpp).
//
// Build note: this translation unit is compiled into the plugin's native
// library on every platform (see src/CMakeLists.txt). zxing-cpp itself is
// pulled in by that CMake file via FetchContent, so no headers are vendored.

#include "ms_zxing.h"
#include "ms_fallback_barcodes.h"

#include <algorithm>
#include <array>
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
constexpr uint32_t kRawMsiPlessey = 262144;
constexpr uint32_t kRawPharmaCode = 524288;
constexpr uint32_t kRawPharmaCodeTwoTrack = 1048576;
constexpr uint32_t kRawUpcEanExtension = 2097152;
constexpr uint32_t kRawGs1Code128 = 4194304;
constexpr uint32_t kRawCode32 = 8388608;
constexpr uint32_t kRawMicroQr = 16777216;
constexpr uint32_t kRawDataBarLimited = 33554432;
constexpr uint32_t kRawRmQr = 67108864;
constexpr uint32_t kRawDxFilmEdge = 134217728;
constexpr uint32_t kRawOtherBarcode = 1073741824;

struct FallbackCandidate {
  int32_t format = -1;
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
  // ITF aliases are normalized to bit 128 by Dart/Kotlin/Swift before this
  // boundary. Values 126/127 here are valid combinations of other format bits.
  BarcodeFormats formats = BarcodeFormat::None;
  if (mask & kRawCode128) formats |= BarcodeFormat::Code128;
  if (mask & kRawGs1Code128) formats |= BarcodeFormat::Code128;
  if (mask & kRawCode39) formats |= BarcodeFormat::Code39;
  if (mask & kRawCode32) formats |= BarcodeFormat::Code39;
  if (mask & kRawCode93) formats |= BarcodeFormat::Code93;
  if (mask & kRawCodabar) formats |= BarcodeFormat::Codabar;
  if (mask & kRawDataMatrix) formats |= BarcodeFormat::DataMatrix;
  if (mask & kRawEan13) formats |= BarcodeFormat::EAN13;
  if (mask & kRawEan8) formats |= BarcodeFormat::EAN8;
  // Dedicated ITF bit only. Legacy aliases 126/127 are not bit flags.
  if (mask & kRawItf) formats |= BarcodeFormat::ITF;
  if (mask & kRawQr) formats |= BarcodeFormat::QRCode;
  if (mask & kRawMicroQr) formats |= BarcodeFormat::MicroQRCode;
  if (mask & kRawRmQr) formats |= BarcodeFormat::RMQRCode;
  if (mask & (kRawDxFilmEdge | kRawOtherBarcode)) formats |= BarcodeFormat::DXFilmEdge;
  if (mask & kRawDataBarLimited) formats |= BarcodeFormat::DataBarLimited;
  if (mask & kRawUpcA) formats |= BarcodeFormat::UPCA;
  if (mask & kRawUpcE) formats |= BarcodeFormat::UPCE;
  if (mask & kRawUpcEanExtension) {
    formats |= BarcodeFormat::EAN13;
    formats |= BarcodeFormat::EAN8;
    formats |= BarcodeFormat::UPCA;
    formats |= BarcodeFormat::UPCE;
  }
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

bool MsiPlesseyRequested(uint32_t mask) {
  return mask == 0 || (mask & kRawMsiPlessey) != 0;
}

bool ExplicitFormatRequested(uint32_t mask, uint32_t format) {
  return mask != 0 && (mask & format) != 0;
}

bool UpcEanRequested(uint32_t mask) {
  return mask == 0 || (mask & (kRawEan13 | kRawEan8 | kRawUpcA | kRawUpcE |
                               kRawUpcEanExtension)) != 0;
}

int Code32AlphabetValue(char c) {
  constexpr std::string_view alphabet = "0123456789BCDFGHJKLMNPQRSTUVWXYZ";
  const size_t index = alphabet.find(c);
  return index == std::string_view::npos ? -1 : static_cast<int>(index);
}

int Code32CheckDigit(std::string_view eightDigits) {
  int sum = 0;
  for (size_t i = 0; i < eightDigits.size(); ++i) {
    int value = eightDigits[i] - '0';
    if (i % 2 == 1) {
      value *= 2;
      value = (value / 10) + (value % 10);
    }
    sum += value;
  }
  return sum % 10;
}

std::optional<std::string> DecodeCode32Text(std::string_view encoded) {
  if (encoded.size() != 6) return std::nullopt;

  uint32_t value = 0;
  for (char c : encoded) {
    const int digit = Code32AlphabetValue(c);
    if (digit < 0) return std::nullopt;
    value = value * 32u + static_cast<uint32_t>(digit);
  }
  if (value > 999999999u) return std::nullopt;

  std::string digits = std::to_string(value);
  if (digits.size() < 9) {
    digits.insert(0, 9 - digits.size(), '0');
  }

  const int expected = Code32CheckDigit(std::string_view(digits).substr(0, 8));
  const int actual = digits[8] - '0';
  if (expected != actual) return std::nullopt;

  return "A" + digits;
}

// Translate a zxing-cpp Barcode back to a Dart rawValue.
int32_t ZxingToRaw(const Barcode& barcode) {
  const BarcodeFormat format = barcode.format();
  if (format == BarcodeFormat::Code128 &&
      barcode.symbologyIdentifier() == "]C1") {
    return kRawGs1Code128;
  }

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
    case BarcodeFormat::MicroQRCode: return kRawMicroQr;
    case BarcodeFormat::RMQRCode: return kRawRmQr;
    case BarcodeFormat::DXFilmEdge: return kRawDxFilmEdge;
    case BarcodeFormat::DataBarLimited: return kRawDataBarLimited;
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
                                            int32_t fixed, bool vertical,
                                            bool trimQuiet = true) {
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

  if (trimQuiet) {
    while (!runs.empty() && !runs.front().black) {
      runs.erase(runs.begin());
    }
    while (!runs.empty() && !runs.back().black) {
      runs.pop_back();
    }
  }
  if (runs.empty()) return std::nullopt;
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

std::optional<double> BlackWideThreshold(const std::vector<Run>& runs) {
  std::vector<int32_t> widths;
  widths.reserve(runs.size());
  for (const Run& run : runs) {
    if (run.black) widths.push_back(run.width);
  }
  if (widths.size() < 2) return std::nullopt;

  int32_t minWidth = widths.front();
  int32_t maxWidth = widths.front();
  for (const int32_t width : widths) {
    minWidth = std::min(minWidth, width);
    maxWidth = std::max(maxWidth, width);
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
    for (const int32_t width : widths) {
      if (width <= threshold) {
        narrowSum += width;
        ++narrowCount;
      } else {
        wideSum += width;
        ++wideCount;
      }
    }
    if (narrowCount == 0 || wideCount == 0) return std::nullopt;
    narrow = narrowSum / narrowCount;
    wide = wideSum / wideCount;
  }

  if (wide / narrow < 1.45 || wide / narrow > 3.5) return std::nullopt;
  return (narrow + wide) / 2.0;
}

std::optional<std::string> DecodePharmacodeOneTrackPattern(
    const std::string& pattern) {
  auto decoded = mobile_scanner::fallback::DecodePharmacodeOneTrack(pattern);
  if (decoded.has_value()) return decoded->text;

  std::string reversed(pattern.rbegin(), pattern.rend());
  decoded = mobile_scanner::fallback::DecodePharmacodeOneTrack(reversed);
  if (decoded.has_value()) return decoded->text;

  return std::nullopt;
}

std::optional<FallbackCandidate> DecodePharmacodeOneTrackRuns(
    const std::vector<Run>& runs, int32_t fixed, bool vertical) {
  if (runs.empty() || !runs.front().black || !runs.back().black) {
    return std::nullopt;
  }

  int blackCount = 0;
  for (size_t i = 0; i < runs.size(); ++i) {
    if (runs[i].black) {
      ++blackCount;
    } else if (i == 0 || i + 1 == runs.size()) {
      return std::nullopt;
    }
  }
  if (blackCount < 2 || blackCount > 16) return std::nullopt;

  const auto threshold = BlackWideThreshold(runs);
  if (!threshold.has_value()) return std::nullopt;

  std::string pattern;
  pattern.reserve(static_cast<size_t>(blackCount));
  for (const Run& run : runs) {
    if (!run.black) continue;
    pattern.push_back(run.width > *threshold ? 'W' : 'N');
  }

  const auto text = DecodePharmacodeOneTrackPattern(pattern);
  if (!text.has_value()) return std::nullopt;

  return FallbackCandidate{
      .format = kRawPharmaCode,
      .text = *text,
      .start = runs.front().start,
      .end = runs.back().start + runs.back().width,
      .fixed = fixed,
      .vertical = vertical,
  };
}

std::vector<bool> BlackColumnsFromRuns(const std::vector<Run>& runs,
                                       int32_t length) {
  std::vector<bool> columns(static_cast<size_t>(length), false);
  for (const Run& run : runs) {
    if (!run.black) continue;
    const int32_t end = std::min(length, run.start + run.width);
    for (int32_t x = std::max(0, run.start); x < end; ++x) {
      columns[static_cast<size_t>(x)] = true;
    }
  }
  return columns;
}

bool HasBlackInRange(const std::vector<bool>& columns, int32_t start,
                     int32_t end) {
  start = std::max(0, start);
  end = std::min(static_cast<int32_t>(columns.size()), end);
  for (int32_t x = start; x < end; ++x) {
    if (columns[static_cast<size_t>(x)]) return true;
  }
  return false;
}

std::optional<std::string> DecodePharmacodeTwoTrackPattern(
    const std::string& pattern) {
  auto decoded = mobile_scanner::fallback::DecodePharmacodeTwoTrack(pattern);
  if (decoded.has_value()) return decoded->text;

  std::string reversed(pattern.rbegin(), pattern.rend());
  decoded = mobile_scanner::fallback::DecodePharmacodeTwoTrack(reversed);
  if (decoded.has_value()) return decoded->text;

  return std::nullopt;
}

std::optional<FallbackCandidate> DecodePharmacodeTwoTrackRows(
    const std::vector<Run>& topRuns, const std::vector<Run>& bottomRuns,
    int32_t length, int32_t topFixed, int32_t bottomFixed, bool vertical) {
  if (length <= 0 || (topRuns.empty() && bottomRuns.empty())) {
    return std::nullopt;
  }

  const std::vector<bool> top = BlackColumnsFromRuns(topRuns, length);
  const std::vector<bool> bottom = BlackColumnsFromRuns(bottomRuns, length);

  std::vector<Run> merged;
  bool current = top[0] || bottom[0];
  int32_t start = 0;
  for (int32_t i = 1; i < length; ++i) {
    const bool black = top[static_cast<size_t>(i)] ||
                       bottom[static_cast<size_t>(i)];
    if (black == current) continue;
    merged.push_back(
        Run{.start = start, .width = i - start, .black = current});
    start = i;
    current = black;
  }
  merged.push_back(
      Run{.start = start, .width = length - start, .black = current});

  while (!merged.empty() && !merged.front().black) {
    merged.erase(merged.begin());
  }
  while (!merged.empty() && !merged.back().black) {
    merged.pop_back();
  }
  if (merged.empty()) return std::nullopt;

  std::string pattern;
  pattern.reserve(merged.size());
  for (const Run& run : merged) {
    if (!run.black) continue;
    const int32_t pad = std::max(1, run.width / 4);
    const bool hasTop =
        HasBlackInRange(top, run.start - pad, run.start + run.width + pad);
    const bool hasBottom =
        HasBlackInRange(bottom, run.start - pad, run.start + run.width + pad);
    if (!hasTop && !hasBottom) return std::nullopt;
    pattern.push_back(hasTop && hasBottom ? '3' : (hasTop ? '2' : '1'));
  }

  if (pattern.size() < 2 || pattern.size() > 16) return std::nullopt;
  const auto text = DecodePharmacodeTwoTrackPattern(pattern);
  if (!text.has_value()) return std::nullopt;

  return FallbackCandidate{
      .format = kRawPharmaCodeTwoTrack,
      .text = *text,
      .start = merged.front().start,
      .end = merged.back().start + merged.back().width,
      .fixed = (topFixed + bottomFixed) / 2,
      .vertical = vertical,
  };
}

std::optional<FallbackCandidate> DecodePharmacodeTwoTrackPair(
    const std::optional<std::vector<Run>>& topRuns,
    const std::optional<std::vector<Run>>& bottomRuns, int32_t length,
    int32_t topFixed, int32_t bottomFixed, bool vertical) {
  if (!topRuns.has_value() && !bottomRuns.has_value()) {
    return std::nullopt;
  }

  const std::vector<Run> empty;
  const std::vector<Run>& top = topRuns.has_value() ? *topRuns : empty;
  const std::vector<Run>& bottom = bottomRuns.has_value() ? *bottomRuns : empty;
  return DecodePharmacodeTwoTrackRows(top, bottom, length, topFixed,
                                      bottomFixed, vertical);
}

std::vector<int32_t> SamplePositions(int32_t length, int32_t sampleCount) {
  std::vector<int32_t> positions;
  positions.reserve(static_cast<size_t>(sampleCount));
  int32_t previous = -1;
  for (int32_t sample = 0; sample < sampleCount; ++sample) {
    const int32_t position = ((sample + 1) * length) / (sampleCount + 1);
    if (position == previous) continue;
    positions.push_back(position);
    previous = position;
  }
  return positions;
}

std::optional<char> DecodeCode11Pattern(std::string_view pattern) {
  for (const auto& symbol : kCode11Symbols) {
    if (symbol.pattern == pattern) return symbol.value;
  }
  return std::nullopt;
}

std::optional<double> Code11WideThreshold(const std::vector<Run>& runs,
                                          size_t start, size_t count) {
  std::vector<int32_t> widths;
  widths.reserve(count);
  for (size_t i = start; i < start + count; ++i) {
    // Inter-character gaps are not part of the Code 11 character pattern and
    // may be wider than a narrow element depending on the renderer.
    if ((i - start) % 6 == 5) continue;
    widths.push_back(runs[i].width);
  }
  if (widths.size() < 10) return std::nullopt;

  int32_t minWidth = widths.front();
  int32_t maxWidth = widths.front();
  for (const int32_t width : widths) {
    minWidth = std::min(minWidth, width);
    maxWidth = std::max(maxWidth, width);
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
    for (const int32_t width : widths) {
      if (width <= threshold) {
        narrowSum += width;
        ++narrowCount;
      } else {
        wideSum += width;
        ++wideCount;
      }
    }
    if (narrowCount == 0 || wideCount == 0) return std::nullopt;
    narrow = narrowSum / narrowCount;
    wide = wideSum / wideCount;
  }

  if (wide / narrow < 1.45 || wide / narrow > 4.5) return std::nullopt;
  return (narrow + wide) / 2.0;
}

std::optional<std::string> DecodeCode11Slice(const std::vector<Run>& runs,
                                             size_t start, size_t count,
                                             bool allowWithoutChecksum) {
  if (count < 23 || count % 6 != 5 || !runs[start].black ||
      !runs[start + count - 1].black) {
    return std::nullopt;
  }

  const auto threshold = Code11WideThreshold(runs, start, count);
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
      if (separator.black) return std::nullopt;
    }
  }

  if (decoded.size() < 4 || decoded.front() != '*' || decoded.back() != '*') {
    return std::nullopt;
  }

  const std::string withChecks = decoded.substr(1, decoded.size() - 2);
  const auto validated =
      mobile_scanner::fallback::ValidateCode11(withChecks);
  if (validated.has_value() && validated->checksumValid) {
    return validated->text;
  }

  if (allowWithoutChecksum) {
    if (withChecks.size() < 4) return std::nullopt;
    return withChecks;
  }

  return std::nullopt;
}

std::optional<FallbackCandidate> DecodeCode11Runs(const std::vector<Run>& runs,
                                                  int32_t fixed,
                                                  bool vertical,
                                                  bool allowWithoutChecksum) {
  std::vector<size_t> starts;
  for (size_t i = 0; i < runs.size(); ++i) {
    if (runs[i].black) starts.push_back(i);
  }

  std::optional<FallbackCandidate> best;
  for (const size_t start : starts) {
    for (size_t count = 23; start + count <= runs.size(); count += 6) {
      auto text = DecodeCode11Slice(runs, start, count, allowWithoutChecksum);
      if (!text.has_value()) {
        std::vector<Run> reversed(runs.begin() + start,
                                  runs.begin() + start + count);
        std::reverse(reversed.begin(), reversed.end());
        int32_t cursor = 0;
        for (auto& run : reversed) {
          run.start = cursor;
          cursor += run.width;
        }
        text = DecodeCode11Slice(reversed, 0, reversed.size(),
                                 allowWithoutChecksum);
      }
      if (!text.has_value()) continue;

      const int32_t startCoord = runs[start].start;
      const Run& last = runs[start + count - 1];
      FallbackCandidate candidate{
          .format = kRawCode11,
          .text = *text,
          .start = startCoord,
          .end = last.start + last.width,
          .fixed = fixed,
          .vertical = vertical,
      };

      if (!best.has_value() || candidate.text.size() > best->text.size()) {
        best = std::move(candidate);
      }
    }
  }
  return best;
}

std::vector<FallbackCandidate> DecodeCode11(const MsZxingDecodeParams& params) {
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

  std::vector<FallbackCandidate> candidates;
  const bool allowWithoutChecksum =
      ExplicitFormatRequested(params.format_mask, kRawCode11);
  const int32_t horizontalSamples =
      params.try_harder != 0 ? 81 : (allowWithoutChecksum ? 61 : 9);
  for (int32_t sample = 0; sample < horizontalSamples; ++sample) {
    const int32_t y =
        ((sample + 1) * height) / (horizontalSamples + 1);
    const auto runs = ExtractRuns(params, originX, originY, width, height, y,
                                  false);
    if (!runs.has_value()) continue;
    auto candidate =
        DecodeCode11Runs(*runs, originY + y, false, allowWithoutChecksum);
    if (!candidate.has_value()) continue;
    candidate->start += originX;
    candidate->end += originX;
    candidates.push_back(*candidate);
    break;
  }

  if (params.try_rotate != 0) {
    const int32_t verticalSamples =
        params.try_harder != 0 ? 81 : (allowWithoutChecksum ? 61 : 9);
    for (int32_t sample = 0; sample < verticalSamples; ++sample) {
      const int32_t x =
          ((sample + 1) * width) / (verticalSamples + 1);
      const auto runs = ExtractRuns(params, originX, originY, width, height, x,
                                    true);
      if (!runs.has_value()) continue;
      auto candidate =
          DecodeCode11Runs(*runs, originX + x, true, allowWithoutChecksum);
      if (!candidate.has_value()) continue;
      candidate->start += originY;
      candidate->end += originY;
      candidates.push_back(*candidate);
      break;
    }
  }

  return candidates;
}

std::optional<char> DecodeMsiDigitModules(std::string_view modules) {
  constexpr std::array<std::string_view, 10> digitModules{{
      "100100100100", "100100100110", "100100110100", "100100110110",
      "100110100100", "100110100110", "100110110100", "100110110110",
      "110100100100", "110100100110",
  }};

  for (size_t i = 0; i < digitModules.size(); ++i) {
    if (modules == digitModules[i]) {
      return static_cast<char>('0' + i);
    }
  }
  return std::nullopt;
}

std::optional<std::string> DecodeMsiModules(std::string modules) {
  if (!modules.starts_with("110") || !modules.ends_with("1001") ||
      modules.size() < 31) {
    return std::nullopt;
  }

  modules = modules.substr(3, modules.size() - 7);
  if (modules.size() % 12 != 0) return std::nullopt;

  std::string digits;
  digits.reserve(modules.size() / 12);
  for (size_t i = 0; i < modules.size(); i += 12) {
    const auto digit = DecodeMsiDigitModules(
        std::string_view(modules).substr(i, 12));
    if (!digit.has_value()) return std::nullopt;
    digits.push_back(*digit);
  }

  const auto validated = mobile_scanner::fallback::ValidateMsiPlesseyAuto(
      digits);
  if (!validated.has_value() || !validated->checksumValid) return std::nullopt;
  // Preserve the complete encoded value for rawValue/rawBytes. The checksum is
  // used to reject false positives, but callers can decide whether to strip it.
  return digits;
}

std::optional<std::string> DecodeMsiSlice(const std::vector<Run>& runs,
                                          size_t start, size_t count) {
  if (count < 13 || !runs[start].black || !runs[start + count - 1].black) {
    return std::nullopt;
  }
  if (start == 0 || start + count >= runs.size()) {
    return std::nullopt;
  }
  if (runs[start - 1].black || runs[start + count].black) {
    return std::nullopt;
  }

  const auto threshold = WideThreshold(runs, start, count);
  if (!threshold.has_value()) return std::nullopt;

  int32_t narrowWidth = runs[start].width;
  for (size_t i = start; i < start + count; ++i) {
    narrowWidth = std::min(narrowWidth, runs[i].width);
  }
  const int32_t minQuietWidth = narrowWidth * 6;
  if (runs[start - 1].width < minQuietWidth ||
      runs[start + count].width < minQuietWidth) {
    return std::nullopt;
  }

  std::string modules;
  modules.reserve(count * 2);
  for (size_t i = start; i < start + count; ++i) {
    modules.append(runs[i].width > *threshold ? 2 : 1,
                   runs[i].black ? '1' : '0');
  }

  auto text = DecodeMsiModules(modules);
  if (text.has_value()) return text;

  std::reverse(modules.begin(), modules.end());
  return DecodeMsiModules(modules);
}

std::optional<FallbackCandidate> DecodeMsiRuns(const std::vector<Run>& runs,
                                               int32_t fixed,
                                               bool vertical) {
  std::optional<FallbackCandidate> best;
  for (size_t start = 0; start < runs.size(); ++start) {
    if (!runs[start].black) continue;
    for (size_t count = 13; start + count <= runs.size(); count += 2) {
      auto text = DecodeMsiSlice(runs, start, count);
      if (!text.has_value()) continue;

      const int32_t startCoord = runs[start].start;
      const Run& last = runs[start + count - 1];
      FallbackCandidate candidate{
          .format = kRawMsiPlessey,
          .text = *text,
          .start = startCoord,
          .end = last.start + last.width,
          .fixed = fixed,
          .vertical = vertical,
      };

      // MSI/Plessey has several checksum variants, so a valid shorter symbol
      // can appear inside a longer one. Prefer the full decoded payload.
      if (!best.has_value() || candidate.text.size() > best->text.size()) {
        best = std::move(candidate);
      }
    }
  }
  return best;
}

std::vector<FallbackCandidate> DecodeMsiPlessey(
    const MsZxingDecodeParams& params) {
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

  std::vector<FallbackCandidate> candidates;
  const int32_t horizontalSamples = params.try_harder != 0 ? 25 : 9;
  for (int32_t sample = 0; sample < horizontalSamples; ++sample) {
    const int32_t y = ((sample + 1) * height) / (horizontalSamples + 1);
    const auto runs =
        ExtractRuns(params, originX, originY, width, height, y, false, false);
    if (!runs.has_value()) continue;
    auto candidate = DecodeMsiRuns(*runs, originY + y, false);
    if (!candidate.has_value()) continue;
    candidate->start += originX;
    candidate->end += originX;
    candidates.push_back(*candidate);
    break;
  }

  if (params.try_rotate != 0) {
    const int32_t verticalSamples = params.try_harder != 0 ? 25 : 9;
    for (int32_t sample = 0; sample < verticalSamples; ++sample) {
      const int32_t x = ((sample + 1) * width) / (verticalSamples + 1);
      const auto runs =
          ExtractRuns(params, originX, originY, width, height, x, true, false);
      if (!runs.has_value()) continue;
      auto candidate = DecodeMsiRuns(*runs, originX + x, true);
      if (!candidate.has_value()) continue;
      candidate->start += originY;
      candidate->end += originY;
      candidates.push_back(*candidate);
      break;
    }
  }

  return candidates;
}

std::vector<FallbackCandidate> DecodePharmacodeOneTrack(
    const MsZxingDecodeParams& params) {
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

  std::vector<FallbackCandidate> candidates;
  const int32_t horizontalSamples = params.try_harder != 0 ? 25 : 9;
  for (int32_t sample = 0; sample < horizontalSamples; ++sample) {
    const int32_t y = ((sample + 1) * height) / (horizontalSamples + 1);
    const auto runs =
        ExtractRuns(params, originX, originY, width, height, y, false);
    if (!runs.has_value()) continue;
    auto candidate = DecodePharmacodeOneTrackRuns(*runs, originY + y, false);
    if (!candidate.has_value()) continue;
    candidate->start += originX;
    candidate->end += originX;
    candidates.push_back(*candidate);
    break;
  }

  if (params.try_rotate != 0) {
    const int32_t verticalSamples = params.try_harder != 0 ? 25 : 9;
    for (int32_t sample = 0; sample < verticalSamples; ++sample) {
      const int32_t x = ((sample + 1) * width) / (verticalSamples + 1);
      const auto runs =
          ExtractRuns(params, originX, originY, width, height, x, true);
      if (!runs.has_value()) continue;
      auto candidate = DecodePharmacodeOneTrackRuns(*runs, originX + x, true);
      if (!candidate.has_value()) continue;
      candidate->start += originY;
      candidate->end += originY;
      candidates.push_back(*candidate);
      break;
    }
  }

  return candidates;
}

std::vector<FallbackCandidate> DecodePharmacodeTwoTrack(
    const MsZxingDecodeParams& params) {
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

  std::vector<FallbackCandidate> candidates;
  constexpr std::array<std::pair<int32_t, int32_t>, 3> samplePairs{{
      {1, 3},
      {1, 4},
      {2, 4},
  }};

  for (const auto& pair : samplePairs) {
    const int32_t topY = (pair.first * height) / 5;
    const int32_t bottomY = (pair.second * height) / 5;
    const auto topRuns =
        ExtractRuns(params, originX, originY, width, height, topY, false);
    const auto bottomRuns =
        ExtractRuns(params, originX, originY, width, height, bottomY, false);
    auto candidate = DecodePharmacodeTwoTrackPair(
        topRuns, bottomRuns, width, originY + topY, originY + bottomY, false);
    if (!candidate.has_value()) continue;
    candidate->start += originX;
    candidate->end += originX;
    candidates.push_back(*candidate);
    break;
  }

  if (candidates.empty()) {
    // Pharmacode Two-Track is usually a small packaging mark. Sparse samples
    // can miss one or both tracks unless the caller provides a tight crop.
    const int32_t horizontalSamples = params.try_harder != 0 ? 61 : 41;
    const int32_t maxSampleGap = params.try_harder != 0 ? 20 : 14;
    const std::vector<int32_t> sampleYs =
        SamplePositions(height, horizontalSamples);
    std::vector<std::optional<std::vector<Run>>> sampleRuns;
    sampleRuns.reserve(sampleYs.size());
    for (const int32_t y : sampleYs) {
      sampleRuns.push_back(
          ExtractRuns(params, originX, originY, width, height, y, false));
    }

    for (size_t top = 0; top + 1 < sampleYs.size() && candidates.empty();
         ++top) {
      const size_t maxBottom =
          std::min(sampleYs.size() - 1, top + static_cast<size_t>(maxSampleGap));
      for (size_t bottom = top + 1; bottom <= maxBottom; ++bottom) {
        auto candidate = DecodePharmacodeTwoTrackPair(
            sampleRuns[top], sampleRuns[bottom], width, originY + sampleYs[top],
            originY + sampleYs[bottom], false);
        if (!candidate.has_value()) continue;
        candidate->start += originX;
        candidate->end += originX;
        candidates.push_back(*candidate);
        break;
      }
    }
  }

  if (params.try_rotate != 0) {
    for (const auto& pair : samplePairs) {
      const int32_t leftX = (pair.first * width) / 5;
      const int32_t rightX = (pair.second * width) / 5;
      const auto leftRuns =
          ExtractRuns(params, originX, originY, width, height, leftX, true);
      const auto rightRuns =
          ExtractRuns(params, originX, originY, width, height, rightX, true);
      auto candidate = DecodePharmacodeTwoTrackPair(
          leftRuns, rightRuns, height, originX + leftX, originX + rightX, true);
      if (!candidate.has_value()) continue;
      candidate->start += originY;
      candidate->end += originY;
      candidates.push_back(*candidate);
      break;
    }

    if (candidates.empty()) {
      const int32_t verticalSamples = params.try_harder != 0 ? 61 : 41;
      const int32_t maxSampleGap = params.try_harder != 0 ? 20 : 14;
      const std::vector<int32_t> sampleXs =
          SamplePositions(width, verticalSamples);
      std::vector<std::optional<std::vector<Run>>> sampleRuns;
      sampleRuns.reserve(sampleXs.size());
      for (const int32_t x : sampleXs) {
        sampleRuns.push_back(
            ExtractRuns(params, originX, originY, width, height, x, true));
      }

      for (size_t left = 0; left + 1 < sampleXs.size() && candidates.empty();
           ++left) {
        const size_t maxRight = std::min(
            sampleXs.size() - 1, left + static_cast<size_t>(maxSampleGap));
        for (size_t right = left + 1; right <= maxRight; ++right) {
          auto candidate = DecodePharmacodeTwoTrackPair(
              sampleRuns[left], sampleRuns[right], height,
              originX + sampleXs[left], originX + sampleXs[right], true);
          if (!candidate.has_value()) continue;
          candidate->start += originY;
          candidate->end += originY;
          candidates.push_back(*candidate);
          break;
        }
      }
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

// The public list is the first member, keeping the original ABI intact.
struct ResultStorage {
  MsZxingResultList list;
  MsZxingResultMetadata* metadata;
  int32_t capacity;
};

void AssignMetadata(MsZxingResultMetadata& metadata, const Barcode& barcode) {
  auto name = ToString(barcode.format());
  if (barcode.format() == BarcodeFormat::QRCode) {
    name = barcode.symbologyIdentifier() == "]Q0" ? "QRCodeModel1" : "QRCodeModel2";
  }
  metadata.format_name = DupString(name);
  metadata.symbology_identifier = DupString(barcode.symbologyIdentifier());
  metadata.version = DupString(barcode.version());
  metadata.error_correction_level = DupString(barcode.ecLevel());
  metadata.sequence_id = DupString(barcode.sequenceId());
  metadata.sequence_size = barcode.sequenceSize();
  metadata.sequence_index = barcode.sequenceIndex();
  metadata.is_inverted = barcode.isInverted() ? 1 : 0;
  metadata.is_mirrored = barcode.isMirrored() ? 1 : 0;
}

}  // namespace

extern "C" {

MsZxingResultList* ms_zxing_decode_sized(const MsZxingDecodeParams* params,
                                        uint64_t data_length) {
  if (params != nullptr && params->width > 0 && params->height > 0 &&
      params->pixel_format >= MS_ZXING_PIXEL_LUM &&
      params->pixel_format <= MS_ZXING_PIXEL_BGRA) {
    const uint64_t rowBytes = static_cast<uint64_t>(params->width) * BytesPerPixel(params->pixel_format);
    const uint64_t stride = params->row_stride == 0 ? rowBytes : params->row_stride;
    const uint64_t required = (static_cast<uint64_t>(params->height) - 1) * stride + rowBytes;
    if (params->row_stride >= 0 && stride >= rowBytes && data_length >= required) {
      return ms_zxing_decode(params);
    }
  }
  return ms_zxing_decode(nullptr);
}

MsZxingResultList* ms_zxing_decode(const MsZxingDecodeParams* params) {
  auto* storage = static_cast<ResultStorage*>(std::calloc(1, sizeof(ResultStorage)));
  if (storage == nullptr) return nullptr;
  auto* list = &storage->list;

  if (params == nullptr || params->data == nullptr || params->width <= 0 ||
      params->height <= 0) {
    return list;  // empty list
  }

  const int32_t bpp = BytesPerPixel(params->pixel_format);
  const int64_t rowBytes = static_cast<int64_t>(params->width) * bpp;
  const bool cropPresent = params->crop_left != 0 || params->crop_top != 0 ||
      params->crop_width != 0 || params->crop_height != 0;
  if (params->pixel_format < MS_ZXING_PIXEL_LUM ||
      params->pixel_format > MS_ZXING_PIXEL_BGRA ||
      rowBytes > INT32_MAX || params->row_stride < 0 ||
      (params->row_stride != 0 && params->row_stride < rowBytes) ||
      params->max_symbols < 0 || params->max_symbols > 255 ||
      (cropPresent && (params->crop_left < 0 || params->crop_top < 0 ||
       params->crop_width <= 0 || params->crop_height <= 0 ||
       static_cast<int64_t>(params->crop_left) + params->crop_width > params->width ||
       static_cast<int64_t>(params->crop_top) + params->crop_height > params->height))) {
    return list;
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
    if (ExplicitFormatRequested(params->format_mask, kRawUpcEanExtension)) {
      options.setEanAddOnSymbol(EanAddOnSymbol::Require);
    } else if (UpcEanRequested(params->format_mask)) {
      options.setEanAddOnSymbol(EanAddOnSymbol::Read);
    }
    const int32_t maxSymbols =
        params->max_symbols > 0 ? params->max_symbols : 255;
    options.setMaxNumberOfSymbols(maxSymbols);

    // ZXing interprets None as its default (all). Skip it when only custom or
    // unavailable formats were requested; custom fallback readers still run.
    const Barcodes barcodes = RawMaskToZxing(params->format_mask) == BarcodeFormat::None
        ? Barcodes{} : ReadBarcodes(image, options);
    std::vector<FallbackCandidate> fallbackCandidates;
    if (Code11Requested(params->format_mask)) {
      const auto code11Candidates = DecodeCode11(*params);
      fallbackCandidates.insert(fallbackCandidates.end(),
                                code11Candidates.begin(),
                                code11Candidates.end());
    }
    if (MsiPlesseyRequested(params->format_mask)) {
      const auto msiCandidates = DecodeMsiPlessey(*params);
      fallbackCandidates.insert(fallbackCandidates.end(),
                                msiCandidates.begin(), msiCandidates.end());
    }
    if (ExplicitFormatRequested(params->format_mask, kRawPharmaCode)) {
      const auto pharmaCandidates = DecodePharmacodeOneTrack(*params);
      fallbackCandidates.insert(fallbackCandidates.end(),
                                pharmaCandidates.begin(),
                                pharmaCandidates.end());
    }
    if (ExplicitFormatRequested(params->format_mask, kRawPharmaCodeTwoTrack)) {
      const auto pharmaTwoCandidates = DecodePharmacodeTwoTrack(*params);
      fallbackCandidates.insert(fallbackCandidates.end(),
                                pharmaTwoCandidates.begin(),
                                pharmaTwoCandidates.end());
    }

    const size_t resultCapacity = barcodes.size() + fallbackCandidates.size();
    if (resultCapacity == 0) {
      return list;
    }

    auto* results = static_cast<MsZxingResult*>(
        std::calloc(resultCapacity, sizeof(MsZxingResult)));
    if (results == nullptr) {
      return list;  // OOM: return empty rather than crash
    }

    storage->metadata = static_cast<MsZxingResultMetadata*>(
        std::calloc(resultCapacity, sizeof(MsZxingResultMetadata)));
    if (storage->metadata == nullptr) {
      std::free(results);
      return list;
    }
    list->results = results;
    storage->capacity = static_cast<int32_t>(resultCapacity);
    for (size_t n = 0; n < resultCapacity; ++n) {
      storage->metadata[n].sequence_size = -1;
      storage->metadata[n].sequence_index = -1;
    }
    int32_t i = 0;
    for (const auto& barcode : barcodes) {
      if (i >= maxSymbols) break;
      if (!barcode.isValid()) continue;

      MsZxingResult& r = results[i];
      int32_t rawFormat = ZxingToRaw(barcode);
      std::string text = barcode.text();
      // A GS1-only request must not produce plain Code 128 results.
      if (params->format_mask != 0 && rawFormat == kRawCode128 &&
          (params->format_mask & kRawGs1Code128) != 0 &&
          (params->format_mask & kRawCode128) == 0) continue;
      bool code32Transformed = false;
      if (rawFormat == kRawCode39 &&
          ExplicitFormatRequested(params->format_mask, kRawCode32)) {
        if (const auto code32Text = DecodeCode32Text(text)) {
          rawFormat = kRawCode32;
          text = *code32Text;
          code32Transformed = true;
        } else if (!ExplicitFormatRequested(params->format_mask, kRawCode39)) {
          continue;
        }
      }

      r.format = rawFormat;
      AssignMetadata(storage->metadata[i], barcode);
      if (code32Transformed) {
        std::free(const_cast<char*>(storage->metadata[i].format_name));
        storage->metadata[i].format_name = DupString("Code32");
      }
      r.text = DupString(text);

      const ByteArray& bytes = barcode.bytes();
      if (code32Transformed) {
        auto* buf = static_cast<uint8_t*>(std::malloc(text.size()));
        if (buf != nullptr) {
          std::memcpy(buf, text.data(), text.size());
          r.bytes = buf;
          r.bytes_length = static_cast<int32_t>(text.size());
        }
      } else if (!bytes.empty()) {
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

    for (const auto& candidate : fallbackCandidates) {
      if (i >= maxSymbols || i >= static_cast<int32_t>(resultCapacity)) break;
      MsZxingResult& r = results[i];
      r.format = candidate.format;
      storage->metadata[i].format_name = DupString(
          candidate.format == kRawCode11 ? "Code11" :
          candidate.format == kRawMsiPlessey ? "MSIPlessey" :
          candidate.format == kRawPharmaCode ? "Pharmacode" : "PharmacodeTwoTrack");
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

const MsZxingResultMetadata* ms_zxing_result_metadata(
    const MsZxingResultList* list, int32_t index) {
  if (list == nullptr || index < 0 || index >= list->count) return nullptr;
  const auto* storage = reinterpret_cast<const ResultStorage*>(list);
  return storage->metadata == nullptr ? nullptr : &storage->metadata[index];
}

void ms_zxing_free_results(MsZxingResultList* list) {
  if (list == nullptr) return;
  auto* storage = reinterpret_cast<ResultStorage*>(list);
  if (list->results != nullptr) {
    for (int32_t i = 0; i < storage->capacity; ++i) {
      std::free(const_cast<char*>(list->results[i].text));
      std::free(const_cast<uint8_t*>(list->results[i].bytes));
    }
    std::free(list->results);
  }
  if (storage->metadata != nullptr) {
    for (int32_t i = 0; i < storage->capacity; ++i) {
      const auto& metadata = storage->metadata[i];
      std::free(const_cast<char*>(metadata.format_name));
      std::free(const_cast<char*>(metadata.symbology_identifier));
      std::free(const_cast<char*>(metadata.version));
      std::free(const_cast<char*>(metadata.error_correction_level));
      std::free(const_cast<char*>(metadata.sequence_id));
    }
    std::free(storage->metadata);
  }
  std::free(storage);
}

const char* ms_zxing_version(void) {
  return ZXING_VERSION_STR;
}

}  // extern "C"
