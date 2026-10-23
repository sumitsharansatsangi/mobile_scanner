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

/// Optional metadata, accessed without changing the original result stride.
final class MsZxingResultMetadata extends ffi.Struct {
  external ffi.Pointer<ffi.Char> formatName;
  external ffi.Pointer<ffi.Char> symbologyIdentifier;
  external ffi.Pointer<ffi.Char> version;
  external ffi.Pointer<ffi.Char> errorCorrectionLevel;
  external ffi.Pointer<ffi.Char> sequenceId;
  @ffi.Int32()
  external int sequenceSize;
  @ffi.Int32()
  external int sequenceIndex;
  @ffi.Int32()
  external int isInverted;
  @ffi.Int32()
  external int isMirrored;
}

typedef MsZxingMetadataNative = ffi.Pointer<MsZxingResultMetadata> Function(
  ffi.Pointer<MsZxingResultList>,
  ffi.Int32,
);
typedef MsZxingMetadataDart = ffi.Pointer<MsZxingResultMetadata> Function(
  ffi.Pointer<MsZxingResultList>,
  int,
);

// Native function typedefs ---------------------------------------------------

typedef MsZxingDecodeNative = ffi.Pointer<MsZxingResultList> Function(
  ffi.Pointer<MsZxingDecodeParams> params,
  ffi.Uint64 dataLength,
);

typedef MsZxingDecodeDart = ffi.Pointer<MsZxingResultList> Function(
  ffi.Pointer<MsZxingDecodeParams> params,
  int dataLength,
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
    : metadata = library.providesSymbol('ms_zxing_result_metadata')
          ? library.lookupFunction<MsZxingMetadataNative, MsZxingMetadataDart>(
              'ms_zxing_result_metadata',
            )
          : null,
      decode = library.lookupFunction<MsZxingDecodeNative, MsZxingDecodeDart>(
        'ms_zxing_decode_sized',
      ),
      freeResults = library.lookupFunction<MsZxingFreeNative, MsZxingFreeDart>(
        'ms_zxing_free_results',
      ),
      version = library
          .lookupFunction<MsZxingVersionNative, MsZxingVersionDart>(
            'ms_zxing_version',
          );

  final MsZxingMetadataDart? metadata;
  final MsZxingDecodeDart decode;
  final MsZxingFreeDart freeResults;
  final MsZxingVersionDart version;
}
