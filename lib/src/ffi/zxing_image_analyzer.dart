/// Selects the native ZXing image analyzer on platforms with `dart:ffi`, and a
/// no-op stub on web. Both expose a top-level `analyzeImageWithZxing(path,
/// formats)` returning the decoded barcodes (empty when unavailable), so the
/// caller can fall back to the platform reader.
library;

export 'zxing_image_analyzer_stub.dart'
    if (dart.library.ffi) 'zxing_image_analyzer_io.dart';
