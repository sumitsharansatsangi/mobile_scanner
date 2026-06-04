// C++20 fallback helpers for barcode formats that are not decoded by
// zxing-cpp, ML Kit, Apple Vision, or ZXing-js.
//
// These helpers intentionally operate on already-normalized symbol streams
// (for example "full/half" postal bars or "wide/narrow" Pharmacode bars).
// They are useful for adding a future detector stage without mixing image
// processing, row segmentation, and payload decoding in one file.

#ifndef MS_FALLBACK_BARCODES_H
#define MS_FALLBACK_BARCODES_H

#include <optional>
#include <span>
#include <string>

namespace mobile_scanner::fallback {

enum class Format {
  msiPlessey,
  code11,
  postnet,
  planet,
  pharmacodeOneTrack,
  pharmacodeTwoTrack,
  intelligentMail,
  royalMail,
  australiaPost,
};

struct DecodeResult {
  Format format;
  std::string text;
  bool checksumValid = false;
  std::string note;
};

// Decode POSTNET from a sequence of full/half bars.
//
// Accepted characters:
// - '1', 'F', 'f', 'T', 't' => full/tall bar
// - '0', 'S', 's', 'H', 'h' => half/short bar
//
// The input may include the leading and trailing frame bars. The returned text
// excludes the checksum digit when the checksum validates.
std::optional<DecodeResult> DecodePostnet(std::string_view bars);

// Decode PLANET from a sequence of full/half bars. Same input conventions as
// DecodePostnet. The returned text excludes the checksum digit when valid.
std::optional<DecodeResult> DecodePlanet(std::string_view bars);

// Decode Pharmacode one-track from a sequence of narrow/wide bars.
//
// Accepted characters:
// - '0', 'N', 'n' => narrow bar
// - '1', 'W', 'w' => wide bar
//
// Spaces are ignored. Inter-bar spaces are not part of the Pharmacode value.
std::optional<DecodeResult> DecodePharmacodeOneTrack(std::string_view bars);

// Decode Pharmacode two-track from a sequence of bar states.
//
// Accepted characters:
// - '1', 'D', 'd', 'B', 'b' => lower/bottom bar
// - '2', 'A', 'a', 'T', 't' => upper/top bar
// - '3', 'F', 'f' => full bar
//
// Spaces are ignored.
std::optional<DecodeResult> DecodePharmacodeTwoTrack(std::string_view bars);

// Validate a decoded MSI / MSI Plessey numeric payload. This does not locate or
// decode bars from an image; it validates already-decoded digits using the
// common Mod-10 or Mod-11 checksum variants.
std::optional<DecodeResult> ValidateMsiPlessey(
    std::string_view digitsWithChecksum,
    bool hasMod10Checksum = true,
    bool hasMod11Checksum = false);

// Validate a decoded MSI / MSI Plessey numeric payload by trying the common
// checksum schemes in order: Mod-10, Mod-10/10, Mod-11, Mod-11/10.
std::optional<DecodeResult> ValidateMsiPlesseyAuto(
    std::string_view digitsWithChecksum);

// Validate a decoded Code 11 payload. Code 11 supports digits and '-'. One C
// checksum is required for short values; a second K checksum is commonly used
// when the payload before checksum is longer than 10 characters.
std::optional<DecodeResult> ValidateCode11(std::string_view textWithChecksum);

// Four-state postal codes such as Intelligent Mail, Royal Mail RM4SCC, and
// Australia Post need a dedicated detector plus complete symbol tables/error
// correction. These functions return a structured "not implemented" result so
// callers can surface an explicit reason instead of silently failing.
DecodeResult DecodeIntelligentMailPlaceholder(std::string_view bars);
DecodeResult DecodeRoyalMailPlaceholder(std::string_view bars);
DecodeResult DecodeAustraliaPostPlaceholder(std::string_view bars);

}  // namespace mobile_scanner::fallback

#endif  // MS_FALLBACK_BARCODES_H
