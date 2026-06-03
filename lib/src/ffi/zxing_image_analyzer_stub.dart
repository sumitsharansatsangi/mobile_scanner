import 'package:mobile_scanner/src/enums/barcode_format.dart';
import 'package:mobile_scanner/src/objects/barcode.dart';

/// Web / no-FFI fallback: the native ZXing engine is unavailable, so report no
/// results and let the caller use its platform reader instead.
Future<List<Barcode>> analyzeImageWithZxing(
  String path,
  List<BarcodeFormat> formats,
) async {
  return const <Barcode>[];
}
