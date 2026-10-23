#include "ms_zxing.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <vector>
#include <string>

int main(int argc, char** argv) {
  if (argc != 3) return 1;
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
  auto* result = ms_zxing_decode_sized_v2(&params, image.size());
  if (!result || result->count != 1 || !result->results[0].text ||
      std::string(result->results[0].text).find("MECARD:") != 0) return 3;
  const auto* metadata = ms_zxing_result_metadata(result, 0);
  if (!metadata || !metadata->format_name ||
      std::string(metadata->format_name) != "QRCodeModel2" ||
      ms_zxing_result_metadata(result, -1) ||
      ms_zxing_result_metadata(result, 1) ||
      ms_zxing_result_metadata(nullptr, 0)) return 14;
  ms_zxing_free_results(result);
  if (std::filesystem::exists("thresholds_new.pnm") ||
      std::filesystem::exists("thresholds.pnm") ||
      std::filesystem::exists("thresholds_avg.pnm")) {
    std::cerr << "Decoder wrote an input-derived debug image\n";
    return 11;
  }
  auto empty = [&](uint64_t length) {
    auto* list = ms_zxing_decode_sized_v2(&params, length);
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
  std::ifstream rmqrInput(argv[2], std::ios::binary);
  rmqrInput >> magic >> width >> height >> maxValue;
  rmqrInput.get();
  if (magic != "P5" || maxValue != 255 || !rmqrInput) return 12;
  const std::vector<uint8_t> rmqr((std::istreambuf_iterator<char>(rmqrInput)), {});
  params.data = rmqr.data(); params.width = width; params.height = height;
  for (const uint32_t mask : {67108864u, 0u, 256u, 131072u}) {
    params.format_mask = mask;
    auto* decoded = ms_zxing_decode_sized_v2(&params, rmqr.size());
    const bool expected = mask == 67108864u || mask == 0u;
    const bool valid = decoded && (expected
        ? decoded->count == 1 && decoded->results[0].format == 67108864 &&
          decoded->results[0].text != nullptr && std::string(decoded->results[0].text) == ",,"
        : decoded->count == 0);
    ms_zxing_free_results(decoded);
    if (!valid) return 13;
  }
  std::cout << "Sized ABI, crop bounds and strict format regressions passed\n";
  return 0;
}
