#include "ms_unsupported_barcodes.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <numeric>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mobile_scanner::unsupported {
namespace {

constexpr std::array<std::pair<std::string_view, char>, 10> kPostnetDigits{{
    {"11000", '0'},
    {"00011", '1'},
    {"00101", '2'},
    {"00110", '3'},
    {"01001", '4'},
    {"01010", '5'},
    {"01100", '6'},
    {"10001", '7'},
    {"10010", '8'},
    {"10100", '9'},
}};

std::optional<char> FullHalfBit(char c) {
  switch (c) {
    case '1':
    case 'F':
    case 'f':
    case 'T':
    case 't':
      return '1';
    case '0':
    case 'S':
    case 's':
    case 'H':
    case 'h':
      return '0';
    case ' ':
    case '\t':
    case '\n':
    case '\r':
    case '|':
      return std::nullopt;
    default:
      return '\xff';
  }
}

std::string NormalizeFullHalf(std::string_view bars) {
  std::string out;
  out.reserve(bars.size());
  for (const char c : bars) {
    const auto bit = FullHalfBit(c);
    if (!bit.has_value()) continue;
    if (*bit == '\xff') return {};
    out.push_back(*bit);
  }
  return out;
}

std::optional<char> DecodePostnetDigit(std::string_view pattern,
                                       bool planet) {
  std::array<char, 5> normalized{};
  for (std::size_t i = 0; i < pattern.size(); ++i) {
    normalized[i] = planet ? (pattern[i] == '1' ? '0' : '1') : pattern[i];
  }
  const std::string_view key(normalized.data(), normalized.size());
  const auto it = std::ranges::find_if(kPostnetDigits, [key](const auto& item) {
    return item.first == key;
  });
  if (it == kPostnetDigits.end()) return std::nullopt;
  return it->second;
}

std::optional<DecodeResult> DecodePostnetLike(std::string_view bars,
                                             bool planet) {
  std::string bits = NormalizeFullHalf(bars);
  if (bits.empty()) return std::nullopt;

  // POSTNET/PLANET symbols are usually framed by one tall/full bar at each end.
  if (bits.size() >= 12 && bits.front() == '1' && bits.back() == '1' &&
      (bits.size() - 2) % 5 == 0) {
    bits = bits.substr(1, bits.size() - 2);
  }

  if (bits.size() < 10 || bits.size() % 5 != 0) return std::nullopt;

  std::string digits;
  digits.reserve(bits.size() / 5);
  for (std::size_t i = 0; i < bits.size(); i += 5) {
    const auto digit = DecodePostnetDigit(std::string_view(bits).substr(i, 5),
                                          planet);
    if (!digit.has_value()) return std::nullopt;
    digits.push_back(*digit);
  }

  const int sum = std::accumulate(digits.begin(), digits.end(), 0,
                                  [](int acc, char c) { return acc + c - '0'; });
  const bool checksumValid = sum % 10 == 0;
  std::string payload = digits;
  if (checksumValid && !payload.empty()) {
    payload.pop_back();
  }

  return DecodeResult{
      .format = planet ? Format::planet : Format::postnet,
      .text = payload,
      .checksumValid = checksumValid,
      .note = checksumValid ? "Checksum digit removed from text."
                            : "Checksum failed; text includes all decoded digits.",
  };
}

std::optional<bool> WideBit(char c) {
  switch (c) {
    case '1':
    case 'W':
    case 'w':
      return true;
    case '0':
    case 'N':
    case 'n':
      return false;
    case ' ':
    case '\t':
    case '\n':
    case '\r':
    case '|':
      return std::nullopt;
    default:
      return std::nullopt;
  }
}

bool IsDigits(std::string_view s) {
  return !s.empty() && std::ranges::all_of(s, [](unsigned char c) {
    return std::isdigit(c) != 0;
  });
}

int MsiMod10CheckDigit(std::string_view payload) {
  std::string doubled;
  doubled.reserve(payload.size() * 2);
  bool doubleDigit = true;
  for (auto it = payload.rbegin(); it != payload.rend(); ++it) {
    const int digit = *it - '0';
    if (doubleDigit) {
      doubled += std::to_string(digit * 2);
    } else {
      doubled.push_back(*it);
    }
    doubleDigit = !doubleDigit;
  }
  const int sum = std::accumulate(doubled.begin(), doubled.end(), 0,
                                  [](int acc, char c) { return acc + c - '0'; });
  return (10 - (sum % 10)) % 10;
}

int MsiMod11CheckDigit(std::string_view payload) {
  int weight = 2;
  int sum = 0;
  for (auto it = payload.rbegin(); it != payload.rend(); ++it) {
    sum += (*it - '0') * weight;
    weight = weight == 7 ? 2 : weight + 1;
  }
  const int check = 11 - (sum % 11);
  return check == 11 ? 0 : check;
}

std::optional<int> Code11Value(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c == '-') return 10;
  return std::nullopt;
}

char Code11Character(int value) {
  return value == 10 ? '-' : static_cast<char>('0' + value);
}

std::optional<char> Code11CheckCharacter(std::string_view payload,
                                         int maxWeight) {
  int weight = 1;
  int sum = 0;
  for (auto it = payload.rbegin(); it != payload.rend(); ++it) {
    const auto value = Code11Value(*it);
    if (!value.has_value()) return std::nullopt;
    sum += *value * weight;
    weight = weight == maxWeight ? 1 : weight + 1;
  }
  return Code11Character(sum % 11);
}

DecodeResult FourStatePlaceholder(Format format, std::string_view bars,
                                  std::string_view name) {
  return DecodeResult{
      .format = format,
      .text = std::string(bars),
      .checksumValid = false,
      .note = std::string(name) +
              " requires a dedicated four-state postal detector and symbol "
              "decoder; this helper only preserves the normalized bar states.",
  };
}

}  // namespace

std::optional<DecodeResult> DecodePostnet(std::string_view bars) {
  return DecodePostnetLike(bars, false);
}

std::optional<DecodeResult> DecodePlanet(std::string_view bars) {
  return DecodePostnetLike(bars, true);
}

std::optional<DecodeResult> DecodePharmacode(std::string_view bars) {
  int value = 0;
  int count = 0;
  for (const char c : bars) {
    const auto wide = WideBit(c);
    if (!wide.has_value()) {
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '|') {
        continue;
      }
      return std::nullopt;
    }
    value = value * 2 + (*wide ? 2 : 1);
    ++count;
  }

  if (count == 0 || value < 3 || value > 131070) return std::nullopt;

  return DecodeResult{
      .format = Format::pharmacode,
      .text = std::to_string(value),
      .checksumValid = true,
      .note = "Pharmacode has no checksum; value range was validated.",
  };
}

std::optional<DecodeResult> ValidateMsiPlessey(
    std::string_view digitsWithChecksum,
    bool hasMod10Checksum,
    bool hasMod11Checksum) {
  if (!IsDigits(digitsWithChecksum)) return std::nullopt;

  std::string payload(digitsWithChecksum);
  bool valid = true;

  if (hasMod10Checksum) {
    if (payload.size() < 2) return std::nullopt;
    const int actual = payload.back() - '0';
    payload.pop_back();
    valid = valid && actual == MsiMod10CheckDigit(payload);
  }

  if (hasMod11Checksum) {
    if (payload.size() < 2) return std::nullopt;
    const int actual = payload.back() - '0';
    payload.pop_back();
    const int expected = MsiMod11CheckDigit(payload);
    if (expected == 10) {
      valid = false;
    } else {
      valid = valid && actual == expected;
    }
  }

  return DecodeResult{
      .format = Format::msiPlessey,
      .text = payload,
      .checksumValid = valid,
      .note = valid ? "Checksum digit(s) removed from text."
                    : "Checksum failed; text contains payload without the "
                      "configured checksum digit(s).",
  };
}

std::optional<DecodeResult> ValidateCode11(std::string_view textWithChecksum) {
  if (textWithChecksum.size() < 2) return std::nullopt;
  if (!std::ranges::all_of(textWithChecksum, [](char c) {
        return Code11Value(c).has_value();
      })) {
    return std::nullopt;
  }

  const auto validate = [](std::string_view value,
                           bool hasK) -> std::optional<std::string> {
    if (value.size() < (hasK ? 3u : 2u)) return std::nullopt;

    std::string payload(value);
    const char actualK = hasK ? payload.back() : '\0';
    if (hasK) payload.pop_back();

    const char actualC = payload.back();
    payload.pop_back();

    const auto expectedC = Code11CheckCharacter(payload, 10);
    if (!expectedC.has_value() || actualC != *expectedC) return std::nullopt;

    if (hasK) {
      std::string payloadWithC = payload;
      payloadWithC.push_back(actualC);
      const auto expectedK = Code11CheckCharacter(payloadWithC, 9);
      if (!expectedK.has_value() || actualK != *expectedK) return std::nullopt;
    }

    return payload;
  };

  const bool expectedK = textWithChecksum.size() > 11;
  std::optional<std::string> payload = validate(textWithChecksum, expectedK);
  if (!payload.has_value()) {
    payload = validate(textWithChecksum, !expectedK);
  }
  const bool valid = payload.has_value();

  return DecodeResult{
      .format = Format::code11,
      .text = valid ? *payload : std::string(textWithChecksum),
      .checksumValid = valid,
      .note = valid ? "Code 11 checksum character(s) removed from text."
                    : "Code 11 checksum failed.",
  };
}

DecodeResult DecodeIntelligentMailPlaceholder(std::string_view bars) {
  return FourStatePlaceholder(Format::intelligentMail, bars,
                              "Intelligent Mail Barcode");
}

DecodeResult DecodeRoyalMailPlaceholder(std::string_view bars) {
  return FourStatePlaceholder(Format::royalMail, bars, "Royal Mail RM4SCC");
}

DecodeResult DecodeAustraliaPostPlaceholder(std::string_view bars) {
  return FourStatePlaceholder(Format::australiaPost, bars, "Australia Post");
}

}  // namespace mobile_scanner::unsupported
