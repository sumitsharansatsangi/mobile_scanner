import 'dart:js_interop';
import 'dart:ui';

import 'package:mobile_scanner/src/enums/barcode_format.dart';
import 'package:mobile_scanner/src/objects/barcode.dart';
import 'package:mobile_scanner/src/objects/start_options.dart';
import 'package:mobile_scanner/src/web/barcode_detector/barcode_detector_formats.dart';
import 'package:mobile_scanner/src/web/barcode_detector/barcode_detector_js.dart';
import 'package:mobile_scanner/src/web/polling_barcode_reader.dart';
import 'package:mobile_scanner/src/web/web_barcode_utils.dart';
import 'package:web/web.dart' as web;

/// A barcode reader that uses the native browser BarcodeDetector API
/// (part of the W3C Shape Detection API).
///
/// Supported browsers: Chrome / Edge 83+, Safari 17+.
/// Firefox does not support this API, use `ZXingWasmBarcodeReader` as a
/// fallback.
final class BarcodeDetectorReader extends PollingBarcodeReader {
  /// Construct a new [BarcodeDetectorReader] instance.
  BarcodeDetectorReader();

  /// Returns `true` when the BarcodeDetector API is available and reports at
  /// least one supported format.
  static Future<bool> isSupported([
    List<BarcodeFormat> formats = const [],
  ]) async {
    try {
      final supported =
          (await NativeBarcodeDetector.getSupportedFormats().toDart).toDart
              .map((s) => s.toDart)
              .toSet();
      if (supported.isEmpty) return false;
      if (formats.isEmpty || formats.contains(BarcodeFormat.all)) return true;
      return formats.every(
        (f) => supported.contains(f.toBarcodeDetectorString),
      );
    } on Object {
      return false;
    }
  }

  NativeBarcodeDetector? _detector;

  /// BarcodeDetector is a native browser API, no external script to load.
  @override
  Future<void> maybeLoadLibrary({String? alternateScriptUrl}) async {}

  @override
  Future<void> prepareDecoder(StartOptions options) async {
    final formats = options.formats;

    final supported = (await NativeBarcodeDetector.getSupportedFormats().toDart)
        .toDart
        .map((s) => s.toDart)
        .toSet();
    _detector = _buildDetector(formats, supported);
  }

  @override
  Future<List<Barcode>> decodeFrame(web.HTMLVideoElement video) async {
    final detector = _detector;

    if (detector == null) {
      return const [];
    }

    final jsResults = await detector.detect(video).toDart;

    return [for (final result in jsResults.toDart) _resultToBarcode(result)];
  }

  @override
  void disposeDecoder() {
    _detector = null;
  }

  NativeBarcodeDetector? _buildDetector(
    List<BarcodeFormat> formats,
    Set<String> supported,
  ) {
    final detectAll = formats.isEmpty || formats.contains(BarcodeFormat.all);

    if (detectAll) {
      return NativeBarcodeDetector();
    }

    final strs = [
      for (final f in formats)
        if (f.toBarcodeDetectorString case final value?
            when supported.contains(value))
          value,
    ];

    if (strs.isEmpty) return null;

    return NativeBarcodeDetector.withOptions(
      BarcodeDetectorInit(
        formats: strs.map((s) => s.toJS).toList().toJS,
      ),
    );
  }

  Barcode _resultToBarcode(DetectedBarcode result) {
    final pts = result.cornerPoints.toDart;
    final corners = [for (final p in pts) Offset(p.x, p.y)];

    final size = computeBoundingBoxSize(corners);
    return Barcode.fromNative({
      'corners': [
        for (final p in corners) {'x': p.dx, 'y': p.dy},
      ],
      'format': result.format.toBarcodeFormat.rawValue,
      'displayValue': result.rawValue,
      'rawValue': result.rawValue,
      'size': {'width': size.width, 'height': size.height},
    });
  }
}
