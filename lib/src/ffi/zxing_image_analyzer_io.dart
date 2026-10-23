import 'dart:io';
import 'dart:isolate';
import 'dart:typed_data';
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
    late final ui.FrameInfo frame;
    try {
      frame = await codec.getNextFrame();
    } finally {
      codec.dispose();
    }
    final image = frame.image;
    final width = image.width;
    final height = image.height;
    final ByteData? byteData;
    try {
      byteData = await image.toByteData();
    } finally {
      image.dispose();
    }

    if (byteData == null) {
      return const <Barcode>[];
    }

    return await _decodePixels(
      byteData.buffer.asUint8List(
        byteData.offsetInBytes,
        byteData.lengthInBytes,
      ),
      width,
      height,
      formats,
    );
  } on Object {
    // Any failure (missing native lib, decode error, unsupported file) falls
    // back to the platform reader via the caller.
    return const <Barcode>[];
  }
}

// Keep the isolate closure separate from image/codec handles, which cannot be
// sent to another isolate. Native scanning does not block the UI isolate.
Future<List<Barcode>> _decodePixels(
  Uint8List pixels,
  int width,
  int height,
  List<BarcodeFormat> formats,
) => Isolate.run(
  () => ZxingDecoder.instance.decode(
    pixels,
    width: width,
    height: height,
    pixelFormat: MsZxingPixelFormat.rgba,
    formats: formats,
    tryHarder: true,
  ),
);
