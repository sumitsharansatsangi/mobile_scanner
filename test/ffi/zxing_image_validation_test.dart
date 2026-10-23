import 'dart:typed_data';
import 'dart:ui';

import 'package:flutter_test/flutter_test.dart';
import 'package:mobile_scanner/src/ffi/zxing_image_validation.dart';

void main() {
  void validate({
    int width = 2,
    int height = 2,
    int stride = 0,
    int format = 0,
    int length = 4,
    Rect? crop,
    int symbols = 0,
  }) => validateZxingImage(
    Uint8List(length),
    width: width,
    height: height,
    rowStride: stride,
    pixelFormat: format,
    maxSymbols: symbols,
    crop: crop,
  );

  test(
    'checks dimensions, layouts, row strides, buffer lengths and symbol counts',
    () {
      expect(() => validate(width: 0), throwsArgumentError);
      expect(() => validate(height: -1), throwsArgumentError);
      expect(() => validate(width: 0x80000000), throwsArgumentError);
      expect(() => validate(format: 4), throwsArgumentError);
      expect(() => validate(format: 2, length: 15), throwsArgumentError);
      expect(() => validate(stride: 1), throwsArgumentError);
      expect(() => validate(stride: -1), throwsArgumentError);
      expect(() => validate(stride: 4, length: 5), throwsArgumentError);
      expect(() => validate(symbols: 256), throwsRangeError);
      expect(() => validate(symbols: -1), throwsRangeError);
      validate();
      validate(stride: 4, length: 6); // Last-row padding is optional.
      validate(format: 2, length: 16);
    },
  );
  test('crop must be finite, nonempty and within image bounds', () {
    expect(
      () => validate(crop: const Rect.fromLTWH(-1, 0, 1, 1)),
      throwsArgumentError,
    );
    expect(
      () => validate(crop: const Rect.fromLTWH(0, 0, 3, 1)),
      throwsArgumentError,
    );
    expect(() => validate(crop: Rect.zero), throwsArgumentError);
    expect(
      () => validate(crop: const Rect.fromLTWH(double.nan, 0, 1, 1)),
      throwsArgumentError,
    );
    validate(crop: const Rect.fromLTWH(1, 1, 1, 1));
  });
}
