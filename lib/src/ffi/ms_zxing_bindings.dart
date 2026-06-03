// Hand-written dart:ffi bindings for the native ZXing-C++ C ABI declared in
// src/ms_zxing.h. The struct layouts here MUST stay byte-compatible with that
// header — field order and types are what map to the C layout.
//
// This is low-level binding code mirroring a C header; the doc/typedef lints
// are intentionally suppressed here as they would be for ffigen output.
// ignore_for_file: public_member_api_docs

import 'dart:ffi' as ffi;

/// Pixel layout constants, mirroring `MsZxingPixelFormat` in ms_zxing.h.
abstract final class MsZxingPixelFormat {
  static const int lum = 0;
  static const int rgb = 1;
  static const int rgba = 2;
  static const int bgra = 3;
}

/// Mirrors `MsZxingPoint`.
final class MsZxingPoint extends ffi.Struct {
  @ffi.Float()
  external double x;

  @ffi.Float()
  external double y;
}

/// Mirrors `MsZxingDecodeParams`.
final class MsZxingDecodeParams extends ffi.Struct {
  external ffi.Pointer<ffi.Uint8> data;

  @ffi.Int32()
  external int width;

  @ffi.Int32()
  external int height;

  @ffi.Int32()
  external int rowStride;

  @ffi.Int32()
  external int pixelFormat;

  @ffi.Uint32()
  external int formatMask;

  @ffi.Int32()
  external int cropLeft;

  @ffi.Int32()
  external int cropTop;

  @ffi.Int32()
  external int cropWidth;

  @ffi.Int32()
  external int cropHeight;

  @ffi.Int32()
  external int tryHarder;

  @ffi.Int32()
  external int tryRotate;

  @ffi.Int32()
  external int tryInvert;

  @ffi.Int32()
  external int tryDownscale;

  @ffi.Int32()
  external int maxSymbols;
}

/// Mirrors `MsZxingResult`.
final class MsZxingResult extends ffi.Struct {
  @ffi.Int32()
  external int format;

  external ffi.Pointer<ffi.Char> text;

  external ffi.Pointer<ffi.Uint8> bytes;

  @ffi.Int32()
  external int bytesLength;

  @ffi.Array<MsZxingPoint>(4)
  external ffi.Array<MsZxingPoint> corners;
}

/// Mirrors `MsZxingResultList`.
final class MsZxingResultList extends ffi.Struct {
  external ffi.Pointer<MsZxingResult> results;

  @ffi.Int32()
  external int count;
}

// Native function typedefs ---------------------------------------------------

typedef MsZxingDecodeNative = ffi.Pointer<MsZxingResultList> Function(
  ffi.Pointer<MsZxingDecodeParams> params,
);

typedef MsZxingDecodeDart = ffi.Pointer<MsZxingResultList> Function(
  ffi.Pointer<MsZxingDecodeParams> params,
);

typedef MsZxingFreeNative = ffi.Void Function(
  ffi.Pointer<MsZxingResultList> list,
);

typedef MsZxingFreeDart = void Function(ffi.Pointer<MsZxingResultList> list);

typedef MsZxingVersionNative = ffi.Pointer<ffi.Char> Function();

typedef MsZxingVersionDart = ffi.Pointer<ffi.Char> Function();

/// Resolves and holds the native `ms_zxing` symbols from the given library.
final class MsZxingBindings {
  /// Looks up the `ms_zxing_*` symbols in [library].
  MsZxingBindings(ffi.DynamicLibrary library)
    : decode = library
          .lookupFunction<MsZxingDecodeNative, MsZxingDecodeDart>(
            'ms_zxing_decode',
          ),
      freeResults = library
          .lookupFunction<MsZxingFreeNative, MsZxingFreeDart>(
            'ms_zxing_free_results',
          ),
      version = library
          .lookupFunction<MsZxingVersionNative, MsZxingVersionDart>(
            'ms_zxing_version',
          );

  final MsZxingDecodeDart decode;
  final MsZxingFreeDart freeResults;
  final MsZxingVersionDart version;
}
