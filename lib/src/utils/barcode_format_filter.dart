import 'package:mobile_scanner/src/enums/barcode_format.dart';

/// Applies an explicit format request, including the engine's ITF aliases.
/// An unsupported or unknown-only request never means "all formats".
bool matchesBarcodeFormat(
  BarcodeFormat actual,
  List<BarcodeFormat> requested, {
  String? rawValue,
}) {
  if (requested.isEmpty || requested.contains(BarcodeFormat.all)) return true;
  if (actual == BarcodeFormat.unknown) return false;
  if (requested.contains(actual)) return true;
  if (actual == BarcodeFormat.gs1Code128 &&
      requested.contains(BarcodeFormat.code128)) {
    return true;
  }
  if (actual == BarcodeFormat.code32 &&
      requested.contains(BarcodeFormat.code39)) {
    return true;
  }
  if (requested.contains(BarcodeFormat.upcEanExtension) &&
      {
        BarcodeFormat.ean13,
        BarcodeFormat.ean8,
        BarcodeFormat.upcA,
        BarcodeFormat.upcE,
      }.contains(actual) &&
      rawValue != null &&
      RegExp(r'^\d{8,13} (?:\d{2}|\d{5})$').hasMatch(rawValue)) {
    return true;
  }
  const itf = {
    BarcodeFormat.itf14,
    BarcodeFormat.itf2of5,
    BarcodeFormat.itf2of5WithChecksum,
  };
  return itf.contains(actual) && requested.any(itf.contains);
}
