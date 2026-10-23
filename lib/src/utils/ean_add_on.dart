import 'package:mobile_scanner/src/enums/barcode_format.dart';

/// Keeps EAN/UPC supplement text consistent across native and web engines.
String? normalizeEanAddOnText(
  String? text,
  String? addOn,
  BarcodeFormat format,
) {
  if (text == null ||
      addOn == null ||
      !RegExp(r'^(?:\d{2}|\d{5})$').hasMatch(addOn)) {
    return text;
  }
  final mainLength = switch (format) {
    BarcodeFormat.ean13 => 13,
    BarcodeFormat.upcA => 12,
    BarcodeFormat.ean8 || BarcodeFormat.upcE => 8,
    _ => 0,
  };
  if (mainLength == 0) return text;
  if (text.length == mainLength) return '$text $addOn';
  if (text.length == mainLength + addOn.length && text.endsWith(addOn)) {
    return '${text.substring(0, mainLength)} $addOn';
  }
  return text;
}
