import 'package:mobile_scanner/src/enums/barcode_format.dart';
import 'package:mobile_scanner/src/enums/web_barcode_reader.dart';
import 'package:mobile_scanner/src/web/barcode_detector/barcode_detector_reader.dart';
import 'package:mobile_scanner/src/web/barcode_reader.dart';
import 'package:mobile_scanner/src/web/zxing/zxing_barcode_reader.dart';
import 'package:mobile_scanner/src/web/zxing_wasm/zxing_wasm_barcode_reader.dart';

/// Selects a reader without narrowing an unrestricted format request to the
/// smaller format set exposed by the browser's native detector.
Future<({BarcodeReader reader, WebBarcodeReader backend})>
createWebBarcodeReader(
  WebBarcodeReader preference,
  List<BarcodeFormat> formats,
) async {
  var backend = preference;
  if (backend == WebBarcodeReader.auto) {
    final restricted =
        formats.isNotEmpty && !formats.contains(BarcodeFormat.all);
    backend = restricted && await BarcodeDetectorReader.isSupported(formats)
        ? WebBarcodeReader.barcodeDetector
        : WebBarcodeReader.zxingWasm;
  }
  final reader = switch (backend) {
    WebBarcodeReader.barcodeDetector => BarcodeDetectorReader(),
    WebBarcodeReader.zxingJs => ZXingBarcodeReader(),
    _ => ZXingWasmBarcodeReader(),
  };
  return (reader: reader, backend: backend);
}
