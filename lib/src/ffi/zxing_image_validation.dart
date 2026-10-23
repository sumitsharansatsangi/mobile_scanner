import 'dart:typed_data';
import 'dart:ui';

import 'package:mobile_scanner/src/ffi/ms_zxing_bindings.dart';

/// Checks buffer bounds before crossing the native ABI boundary.
void validateZxingImage(
  Uint8List data, {
  required int width,
  required int height,
  required int rowStride,
  required int pixelFormat,
  required int maxSymbols,
  Rect? crop,
}) {
  const maxInt32 = 0x7fffffff;
  if (width <= 0 || height <= 0 || width > maxInt32 || height > maxInt32) {
    throw ArgumentError(
      'Image dimensions must be positive signed 32-bit integers.',
    );
  }
  final bytesPerPixel = switch (pixelFormat) {
    MsZxingPixelFormat.lum => 1,
    MsZxingPixelFormat.rgb => 3,
    MsZxingPixelFormat.rgba || MsZxingPixelFormat.bgra => 4,
    _ => throw ArgumentError.value(
      pixelFormat,
      'pixelFormat',
      'Unknown pixel layout',
    ),
  };
  final rowBytes = width * bytesPerPixel;
  final stride = rowStride == 0 ? rowBytes : rowStride;
  if (rowBytes > maxInt32 || stride < rowBytes || stride > maxInt32) {
    throw ArgumentError.value(
      rowStride,
      'rowStride',
      'Must fit a complete pixel row',
    );
  }
  final requiredBytes = (height - 1) * stride + rowBytes;
  if (data.length < requiredBytes) {
    throw ArgumentError.value(
      data.length,
      'data.length',
      'Image requires $requiredBytes bytes',
    );
  }
  if (maxSymbols < 0 || maxSymbols > 255) {
    throw RangeError.range(maxSymbols, 0, 255, 'maxSymbols');
  }
  if (crop != null &&
      (!crop.isFinite ||
          crop.left < 0 ||
          crop.top < 0 ||
          crop.width < 1 ||
          crop.height < 1 ||
          crop.right > width ||
          crop.bottom > height)) {
    throw ArgumentError.value(
      crop,
      'crop',
      'Must lie within the image and contain whole pixels',
    );
  }
}
