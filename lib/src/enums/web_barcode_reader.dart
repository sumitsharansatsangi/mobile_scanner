/// The barcode detection backend to use on the web platform.
enum WebBarcodeReader {
  /// Automatically select the best available reader.
  ///
  /// Uses zxing-wasm when all formats are requested. For an explicit format
  /// list, uses the native BarcodeDetector API if it supports every requested
  /// format, otherwise uses zxing-wasm.
  auto,

  /// Force the native BarcodeDetector API.
  ///
  /// Throws a `MobileScannerException` during `start()` if the API is not
  /// available in the current browser.
  barcodeDetector,

  /// Force the zxing-wasm (zxing-cpp WASM) reader.
  ///
  /// Works in all browsers that support WebAssembly (effectively all modern
  /// browsers).
  zxingWasm,

  /// Force the legacy `@zxing/library` JavaScript reader.
  ///
  /// This is a pure-JS port of ZXing.
  /// Prefer [zxingWasm] or [barcodeDetector] instead.
  zxingJs;

  /// A human-readable display name for this reader, suitable for use in UI
  /// labels and dialogs.
  String get label => switch (this) {
    WebBarcodeReader.auto => 'Auto',
    WebBarcodeReader.barcodeDetector => 'Native BarcodeDetector',
    WebBarcodeReader.zxingWasm => 'zxing-wasm',
    WebBarcodeReader.zxingJs => 'ZXing-js (legacy)',
  };

  /// A short display name for this reader, suitable for use in compact UI
  /// elements such as menu items or badges.
  String get shortLabel => switch (this) {
    WebBarcodeReader.auto => 'Auto',
    WebBarcodeReader.barcodeDetector => 'Native',
    WebBarcodeReader.zxingWasm => 'WASM',
    WebBarcodeReader.zxingJs => 'JS',
  };
}
