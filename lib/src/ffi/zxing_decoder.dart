import 'dart:ffi' as ffi;
import 'dart:io' show Platform;
import 'dart:typed_data';
import 'dart:ui' show Offset, Rect, Size;

import 'package:ffi/ffi.dart';
import 'package:mobile_scanner/src/enums/barcode_format.dart';
import 'package:mobile_scanner/src/ffi/ms_zxing_bindings.dart';
import 'package:mobile_scanner/src/objects/barcode.dart';
import 'package:mobile_scanner/src/objects/barcode_bytes.dart';

/// Decodes barcodes by calling the native ZXing-C++ engine through dart:ffi.
///
/// This is the primary engine on desktop (Linux/Windows) and the engine used
/// for `analyzeImage` on all FFI-capable platforms. The live-camera hot path on
/// Android/iOS calls the same native C ABI directly from Kotlin/Swift instead,
/// so it does not go through this class.
final class ZxingDecoder {
  ZxingDecoder._(this._bindings);

  final MsZxingBindings _bindings;

  static ZxingDecoder? _instance;

  /// The shared decoder instance, lazily loading the native library on first
  /// use. Throws [UnsupportedError] on platforms without the native library
  /// (e.g. web, which uses the ZXing-js reader instead).
  // ignore: prefer_constructors_over_static_methods
  static ZxingDecoder get instance =>
      _instance ??= ZxingDecoder._(MsZxingBindings(_openLibrary()));

  /// The linked zxing-cpp version string, for diagnostics.
  String get nativeVersion =>
      _bindings.version().cast<Utf8>().toDartString();

  static ffi.DynamicLibrary _openLibrary() {
    if (Platform.isAndroid || Platform.isLinux) {
      return ffi.DynamicLibrary.open('libms_zxing.so');
    }
    if (Platform.isWindows) {
      return ffi.DynamicLibrary.open('ms_zxing.dll');
    }
    if (Platform.isMacOS || Platform.isIOS) {
      // On Apple platforms the symbols are linked into the app/process.
      return ffi.DynamicLibrary.process();
    }
    throw UnsupportedError(
      'The native ZXing engine is not available on this platform.',
    );
  }

  /// Decodes all barcodes found in a raw image buffer.
  ///
  /// [data] is the pixel buffer described by [width], [height], [rowStride]
  /// (bytes per row; 0 means tightly packed) and [pixelFormat] (one of
  /// [MsZxingPixelFormat]). [formats] narrows the search; an empty list (or one
  /// containing [BarcodeFormat.all]) searches every supported format.
  ///
  /// Pass [crop] to restrict scanning to a scan window (in image pixels);
  /// returned corners are still mapped back to full-image coordinates. Set
  /// [tryHarder] for the slower, more robust scan used by retries and
  /// `analyzeImage`.
  List<Barcode> decode(
    Uint8List data, {
    required int width,
    required int height,
    int rowStride = 0,
    int pixelFormat = MsZxingPixelFormat.lum,
    List<BarcodeFormat> formats = const <BarcodeFormat>[],
    Rect? crop,
    bool tryHarder = false,
    bool tryRotate = false,
    bool tryInvert = false,
    bool tryDownscale = false,
    int maxSymbols = 0,
  }) {
    final dataPtr = calloc<ffi.Uint8>(data.length);
    final paramsPtr = calloc<MsZxingDecodeParams>();
    try {
      dataPtr.asTypedList(data.length).setAll(0, data);

      paramsPtr.ref
        ..data = dataPtr
        ..width = width
        ..height = height
        ..rowStride = rowStride
        ..pixelFormat = pixelFormat
        ..formatMask = _formatMask(formats)
        ..cropLeft = crop?.left.toInt() ?? 0
        ..cropTop = crop?.top.toInt() ?? 0
        ..cropWidth = crop?.width.toInt() ?? 0
        ..cropHeight = crop?.height.toInt() ?? 0
        ..tryHarder = tryHarder ? 1 : 0
        ..tryRotate = tryRotate ? 1 : 0
        ..tryInvert = tryInvert ? 1 : 0
        ..tryDownscale = tryDownscale ? 1 : 0
        ..maxSymbols = maxSymbols;

      final listPtr = _bindings.decode(paramsPtr);

      if (listPtr == ffi.nullptr) {
        return const <Barcode>[];
      }

      try {
        final imageSize = Size(width.toDouble(), height.toDouble());
        return _readResults(listPtr.ref, imageSize);
      } finally {
        _bindings.freeResults(listPtr);
      }
    } finally {
      calloc
        ..free(dataPtr)
        ..free(paramsPtr);
    }
  }

  static int _formatMask(List<BarcodeFormat> formats) {
    if (formats.isEmpty || formats.contains(BarcodeFormat.all)) {
      return 0; // 0 == search all formats
    }
    var mask = 0;
    for (final format in formats) {
      if (format == BarcodeFormat.unknown) continue;
      mask |= _maskBit(format);
    }
    return mask;
  }

  /// The bitmask contribution for [format].
  ///
  /// Most [BarcodeFormat.rawValue]s are already power-of-two flags, but
  /// [BarcodeFormat.itf2of5] (126) and [BarcodeFormat.itf2of5WithChecksum]
  /// (127) are not: their values collide with combinations of the other flags
  /// (126 == code39|code93|codabar|dataMatrix|ean13|ean8). OR-ing them raw both
  /// enables those unrelated formats and, on the native side, can spuriously
  /// turn on ITF for plain linear requests. Normalize them onto the dedicated
  /// ITF bit ([BarcodeFormat.itf14] == 128). zxing-cpp exposes a single ITF
  /// format, so the variant is not distinguished at decode time anyway.
  static int _maskBit(BarcodeFormat format) {
    if (format == BarcodeFormat.itf2of5 ||
        format == BarcodeFormat.itf2of5WithChecksum) {
      return BarcodeFormat.itf14.rawValue;
    }
    return format.rawValue;
  }

  static List<Barcode> _readResults(MsZxingResultList list, Size imageSize) {
    final barcodes = <Barcode>[];
    for (var i = 0; i < list.count; i++) {
      final result = list.results[i];

      final text = result.text == ffi.nullptr
          ? null
          : result.text.cast<Utf8>().toDartString();

      BarcodeBytes? rawBytes;
      if (result.bytes != ffi.nullptr && result.bytesLength > 0) {
        // Copy out of native memory before the list is freed.
        rawBytes = DecodedBarcodeBytes(
          bytes: Uint8List.fromList(
            result.bytes.asTypedList(result.bytesLength),
          ),
        );
      }

      final corners = <Offset>[
        for (var c = 0; c < 4; c++)
          Offset(result.corners[c].x, result.corners[c].y),
      ];

      barcodes.add(
        Barcode(
          format: BarcodeFormat.fromRawValue(result.format),
          rawValue: text,
          displayValue: text,
          rawDecodedBytes: rawBytes,
          corners: corners,
          size: imageSize,
        ),
      );
    }
    return barcodes;
  }
}
