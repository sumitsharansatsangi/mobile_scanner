#include "ms_zxing.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <vector>
#include <string>

int main(int argc, char** argv) {
  if (argc != 2) return 1;
  std::ifstream input(argv[1], std::ios::binary);
  std::string magic;
  int width = 0, height = 0, maxValue = 0;
  input >> magic >> width >> height >> maxValue;
  input.get();
  if (magic != "P5" || maxValue != 255 || !input) return 2;
  const std::vector<uint8_t> image((std::istreambuf_iterator<char>(input)), {});
  MsZxingDecodeParams params{};
  params.data = image.data(); params.width = width; params.height = height;
  params.try_harder = 1; params.try_rotate = 1;
  params.format_mask = 256; // QR
  auto* result = ms_zxing_decode_sized(&params, image.size());
  if (!result || result->count != 1 || !result->results[0].text ||
      std::string(result->results[0].text).find("MECARD:") != 0) return 3;
  ms_zxing_free_results(result);
  if (std::filesystem::exists("thresholds_new.pnm") ||
      std::filesystem::exists("thresholds.pnm") ||
      std::filesystem::exists("thresholds_avg.pnm")) {
    std::cerr << "Decoder wrote an input-derived debug image\n";
    return 11;
  }
  auto empty = [&](uint64_t length) {
    auto* list = ms_zxing_decode_sized(&params, length);
    const bool valid = list && list->count == 0;
    ms_zxing_free_results(list);
    return valid;
  };
  if (!empty(image.size() - 1)) return 4;
  params.row_stride = width - 1;
  if (!empty(image.size())) return 5;
  params.row_stride = 0; params.pixel_format = 99;
  if (!empty(image.size())) return 6;
  params.pixel_format = 0; params.crop_width = width + 1; params.crop_height = height;
  if (!empty(image.size())) return 7;
  params.crop_width = 0; params.crop_height = 0;
  params.max_symbols = 256;
  if (!empty(image.size())) return 8;
  params.max_symbols = 0;
  // ZXing's empty supported mask must not silently activate all readers.
  params.format_mask = 65536; // unavailable DotCode
  if (!empty(image.size())) return 9;
  params.format_mask = 131072; // custom Code11 only, not QR
  if (!empty(image.size())) return 10;
  std::cout << "Sized ABI, crop bounds and strict format regressions passed\n";
  return 0;
}
