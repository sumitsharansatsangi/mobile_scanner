import 'package:mobile_scanner/src/enums/barcode_format.dart';
import 'package:mobile_scanner/src/objects/barcode_capture.dart';
import 'package:mobile_scanner/src/utils/barcode_format_filter.dart';

/// Enforces the request even when a recovery decoder scans a wider set.
BarcodeCapture? filterBarcodeCapture(
  BarcodeCapture? capture,
  List<BarcodeFormat> formats,
) {
  if (capture == null) return null;
  final barcodes = capture.barcodes
      .where(
        (b) => matchesBarcodeFormat(b.format, formats, rawValue: b.rawValue),
      )
      .toList();
  if (barcodes.isEmpty) return null;
  if (barcodes.length == capture.barcodes.length) return capture;
  return BarcodeCapture(
    barcodes: barcodes,
    image: capture.image,
    imageQuality: capture.imageQuality,
    raw: capture.raw,
    size: capture.size,
  );
}
