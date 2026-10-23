import 'dart:convert';
import 'dart:js_interop';
import 'dart:ui';

import 'package:mobile_scanner/src/enums/barcode_format.dart';
import 'package:mobile_scanner/src/objects/barcode.dart';
import 'package:mobile_scanner/src/utils/ean_add_on.dart';
import 'package:mobile_scanner/src/web/web_barcode_utils.dart';
import 'package:mobile_scanner/src/web/zxing_wasm/zxing_wasm_formats.dart';
import 'package:web/web.dart' as web;

/// JS interop for the `window.ZXingWASM` global set by the zxing-wasm IIFE
/// build.
///
/// See https://github.com/Sec-ant/zxing-wasm
@JS('ZXingWASM')
external ZXingWasmModule get zxingWasmModule;

/// The `ZXingWASM` global object exposed by the IIFE reader build.
@JS()
extension type ZXingWasmModule(JSObject _) implements JSObject {
  /// Detect barcodes in [imageData] captured from a canvas.
  ///
  /// Returns a [JSPromise] that resolves to a list of [ZXingWasmReadResult]s.
  external JSPromise<JSArray<ZXingWasmReadResult>> readBarcodes(
    web.ImageData imageData,
    ZXingWasmReaderOptions options,
  );
}

/// Options passed to [ZXingWasmModule.readBarcodes].
@JS()
extension type ZXingWasmReaderOptions._(JSObject _) implements JSObject {
  /// Creates reader options that detect all supported barcode formats.
  external factory ZXingWasmReaderOptions({
    /// Spend more time to improve detection accuracy. Defaults to `true`.
    bool tryHarder,

    /// Also try 90°/180°/270° rotations. Defaults to `true`.
    bool tryRotate,

    /// Also try inverted/reversed reflectance. Defaults to `false`.
    bool tryInvert,

    /// Read or require EAN-2/EAN-5 supplements alongside the main symbol.
    String eanAddOnSymbol,
  });

  /// Creates reader options restricted to the given [formats].
  ///
  /// Pass a non-empty list of format strings (e.g. `['QRCode', 'EAN13']`).
  /// Passing an empty list or omitting formats detects all supported formats.
  external factory ZXingWasmReaderOptions.withFormats({
    /// Barcode format filter.
    JSArray<JSString> formats,

    /// Spend more time to improve detection accuracy. Defaults to `true`.
    bool tryHarder,

    /// Also try 90°/180°/270° rotations. Defaults to `true`.
    bool tryRotate,

    /// Also try inverted/reversed reflectance. Defaults to `false`.
    bool tryInvert,

    /// Read or require EAN-2/EAN-5 supplements alongside the main symbol.
    String eanAddOnSymbol,
  });
}

/// A single barcode result returned by [ZXingWasmModule.readBarcodes].
@JS()
extension type ZXingWasmReadResult(JSObject _) implements JSObject {
  /// Decoded text content.
  external String? get text;

  /// Canonical barcode format name, e.g. `'QRCode'`, `'EAN13'`, `'Code128'`.
  ///
  /// May be a sub-variant name, e.g. `'ITF14'`, `'ISBN'` or `'AztecCode'`.
  external String get format;

  /// AIM identifier reported by the engine.
  external String? get symbologyIdentifier;

  /// Symbol version or dimensions.
  external String? get version;

  /// Error correction level.
  external String? get ecLevel;

  /// Reversed reflectance and mirrored-symbol indicators.
  external bool? get isInverted;

  /// Whether the symbol was mirrored.
  external bool? get isMirrored;

  /// Structured append sequence details.
  external int? get sequenceSize;

  /// Zero-based index in a structured-append sequence.
  external int? get sequenceIndex;

  /// Identifier shared by structured-append symbols.
  external String? get sequenceId;

  /// JSON object with additional engine details.
  external String? get extra;

  /// Raw barcode bytes as returned by the decoder.
  external JSUint8Array? get bytes;

  /// Corner positions in image pixel coordinates.
  external ZXingWasmPosition get position;

  /// Whether the result passed all integrity checks (checksum, etc.).
  external bool get isValid;
}

/// Corner positions of a detected barcode in image pixel coordinates.
@JS()
extension type ZXingWasmPosition(JSObject _) implements JSObject {
  /// Top-left corner, or `null` if not available.
  external ZXingWasmPoint? get topLeft;

  /// Top-right corner, or `null` if not available.
  external ZXingWasmPoint? get topRight;

  /// Bottom-right corner, or `null` if not available.
  external ZXingWasmPoint? get bottomRight;

  /// Bottom-left corner, or `null` if not available.
  external ZXingWasmPoint? get bottomLeft;
}

/// A 2-D point in image pixel coordinates, used for barcode corner positions.
@JS()
extension type ZXingWasmPoint(JSObject _) implements JSObject {
  /// Horizontal coordinate in image pixel space.
  external double get x;

  /// Vertical coordinate in image pixel space.
  external double get y;
}

/// Converts a [ZXingWasmReadResult] to a [Barcode].
extension ZXingWasmReadResultToBarcode on ZXingWasmReadResult {
  /// Converts this result to a [Barcode].
  Barcode get toBarcode {
    final pos = position;
    final corners = <Offset>[];

    final tl = pos.topLeft;
    final tr = pos.topRight;
    final br = pos.bottomRight;
    final bl = pos.bottomLeft;

    if (tl != null && tr != null && br != null && bl != null) {
      corners.addAll([
        Offset(tl.x, tl.y),
        Offset(tr.x, tr.y),
        Offset(br.x, br.y),
        Offset(bl.x, bl.y),
      ]);
    }

    final rawBytes = bytes?.toDart;

    final size = computeBoundingBoxSize(corners);
    var details = <String, dynamic>{};
    final encoded = extra;
    if (encoded != null && encoded.isNotEmpty) {
      try {
        final decoded = jsonDecode(encoded);
        if (decoded is Map<String, dynamic>) details = decoded;
      } on FormatException {
        // Custom older modules may not return JSON metadata.
      }
    }
    final addon = details['EanAddOn'] as String?;
    final value = normalizeEanAddOnText(text, addon, format.toBarcodeFormat);
    final variant = format == 'QRCode' && symbologyIdentifier == ']Q0'
        ? 'QRCodeModel1'
        : format;

    return Barcode.fromNative({
      'corners': [
        for (final p in corners) {'x': p.dx, 'y': p.dy},
      ],
      'format':
          (format == 'Code128' && symbologyIdentifier == ']C1'
                  ? BarcodeFormat.gs1Code128
                  : format.toBarcodeFormat)
              .rawValue,
      'metadata': {
        'formatName': variant,
        'symbologyIdentifier': symbologyIdentifier,
        'version': version ?? details['Version']?.toString(),
        'errorCorrectionLevel': ecLevel ?? details['ECLevel']?.toString(),
        'isInverted': isInverted,
        'isMirrored': isMirrored,
        'sequenceSize': sequenceSize,
        'sequenceIndex': sequenceIndex,
        'sequenceId': sequenceId,
      },
      'displayValue': value,
      'rawValue': value,
      'rawBytes': rawBytes,
      'size': {'width': size.width, 'height': size.height},
    });
  }
}
