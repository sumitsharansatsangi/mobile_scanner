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
  if (requested.contains(actual)) {
    return actual != BarcodeFormat.itf2of5WithChecksum ||
        (rawValue != null && _hasMod10Checksum(rawValue));
  }
  if (actual == BarcodeFormat.gs1Code128 &&
      requested.contains(BarcodeFormat.code128)) {
    return true;
  }
  if (actual == BarcodeFormat.dxFilmEdge &&
      requested.contains(BarcodeFormat.otherBarcode)) {
    return true;
  }
  if (requested.contains(BarcodeFormat.eanUpc) &&
      {
        BarcodeFormat.ean13,
        BarcodeFormat.ean8,
        BarcodeFormat.upcA,
        BarcodeFormat.upcE,
        BarcodeFormat.eanUpc,
      }.contains(actual)) {
    return true;
  }
  if (actual == BarcodeFormat.pzn && requested.contains(BarcodeFormat.code39)) {
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
    BarcodeFormat.itf,
    BarcodeFormat.itf14,
    BarcodeFormat.itf2of5,
    BarcodeFormat.itf2of5WithChecksum,
  };
  if (itf.contains(actual) && requested.any(itf.contains)) {
    if (requested.every(
      (f) => f == BarcodeFormat.itf2of5WithChecksum || !itf.contains(f),
    )) {
      return rawValue != null && _hasMod10Checksum(rawValue);
    }
    return true;
  }
  return false;
}

bool _hasMod10Checksum(String value) {
  if (!RegExp(r'^\d{2,}$').hasMatch(value)) return false;
  var sum = 0;
  var weight = 3;
  for (var i = value.length - 2; i >= 0; i--) {
    sum += (value.codeUnitAt(i) - 48) * weight;
    weight = weight == 3 ? 1 : 3;
  }
  return (10 - sum % 10) % 10 == value.codeUnitAt(value.length - 1) - 48;
}
