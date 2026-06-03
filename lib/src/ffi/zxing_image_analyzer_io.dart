import 'dart:io';
import 'dart:ui' as ui;

import 'package:mobile_scanner/src/enums/barcode_format.dart';
import 'package:mobile_scanner/src/ffi/ms_zxing_bindings.dart';
import 'package:mobile_scanner/src/ffi/zxing_decoder.dart';
import 'package:mobile_scanner/src/objects/barcode.dart';

/// Decodes the image at [path] with the native ZXing-C++ engine.
///
/// Returns an empty list if the file can't be read/decoded or the native engine
/// is unavailable on this platform (e.g. iOS/macOS without the
/// `MOBILE_SCANNER_ZXING` build flag), so the caller can fall back to the
/// platform reader. Uses the thorough (`tryHarder`) scan since latency is not
/// critical for a one-shot image analysis.
Future<List<Barcode>> analyzeImageWithZxing(
  String path,
  List<BarcodeFormat> formats,
) async {
  try {
    final bytes = await File(path).readAsBytes();
    final codec = await ui.instantiateImageCodec(bytes);
    final frame = await codec.getNextFrame();
    final image = frame.image;

    // rawRgba is toByteData's default format; passing it would be redundant.
    final byteData = await image.toByteData();
    final width = image.width;
    final height = image.height;
    image.dispose();

    if (byteData == null) {
      return const <Barcode>[];
    }

    return ZxingDecoder.instance.decode(
      byteData.buffer.asUint8List(),
      width: width,
      height: height,
      pixelFormat: MsZxingPixelFormat.rgba,
      formats: formats,
      tryHarder: true,
    );
  } on Object {
    // Any failure (missing native lib, decode error, unsupported file) falls
    // back to the platform reader via the caller.
    return const <Barcode>[];
  }
}
