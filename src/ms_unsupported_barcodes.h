// C++20 helpers for barcode formats that are not exposed through
// mobile_scanner's BarcodeFormat enum or zxing-cpp integration.
//
// These helpers intentionally operate on already-normalized symbol streams
// (for example "full/half" postal bars or "wide/narrow" Pharmacode bars).
// They are useful for adding a future detector stage without mixing image
// processing, row segmentation, and payload decoding in one file.

#ifndef MS_UNSUPPORTED_BARCODES_H
#define MS_UNSUPPORTED_BARCODES_H

#include <optional>
#include <span>
#include <string>

namespace mobile_scanner::unsupported {

enum class Format {
  msiPlessey,
  code11,
  postnet,
  planet,
  pharmacode,
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

// Decode Pharmacode from a sequence of narrow/wide bars.
//
// Accepted characters:
// - '0', 'N', 'n' => narrow bar
// - '1', 'W', 'w' => wide bar
//
// Spaces are ignored. Inter-bar spaces are not part of the Pharmacode value.
std::optional<DecodeResult> DecodePharmacode(std::string_view bars);

// Validate a decoded MSI / MSI Plessey numeric payload. This does not locate or
// decode bars from an image; it validates already-decoded digits using the
// common Mod-10 or Mod-11 checksum variants.
std::optional<DecodeResult> ValidateMsiPlessey(
    std::string_view digitsWithChecksum,
    bool hasMod10Checksum = true,
    bool hasMod11Checksum = false);

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

}  // namespace mobile_scanner::unsupported

#endif  // MS_UNSUPPORTED_BARCODES_H
