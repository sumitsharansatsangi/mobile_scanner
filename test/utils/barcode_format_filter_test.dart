import 'package:flutter_test/flutter_test.dart';
import 'package:mobile_scanner/mobile_scanner.dart';
import 'package:mobile_scanner/src/utils/barcode_format_filter.dart';

void main() {
  test('unsupported and unknown-only requests never become all', () {
    expect(
      matchesBarcodeFormat(BarcodeFormat.qrCode, [BarcodeFormat.dotCode]),
      isFalse,
    );
    expect(
      matchesBarcodeFormat(BarcodeFormat.qrCode, [BarcodeFormat.unknown]),
      isFalse,
    );
    expect(
      matchesBarcodeFormat(BarcodeFormat.unknown, [BarcodeFormat.qrCode]),
      isFalse,
    );
    expect(matchesBarcodeFormat(BarcodeFormat.qrCode, []), isTrue);
    expect(
      matchesBarcodeFormat(BarcodeFormat.qrCode, [BarcodeFormat.all]),
      isTrue,
    );
  });
  test(
    'mixed requests stay narrow and ITF aliases share the engine format',
    () {
      expect(
        matchesBarcodeFormat(BarcodeFormat.qrCode, [
          BarcodeFormat.dotCode,
          BarcodeFormat.ean13,
        ]),
        isFalse,
      );
      expect(
        matchesBarcodeFormat(BarcodeFormat.ean13, [
          BarcodeFormat.dotCode,
          BarcodeFormat.ean13,
        ]),
        isTrue,
      );
      expect(
        matchesBarcodeFormat(BarcodeFormat.itf14, [BarcodeFormat.itf2of5]),
        isTrue,
      );
      expect(
        matchesBarcodeFormat(BarcodeFormat.gs1Code128, [BarcodeFormat.code128]),
        isTrue,
      );
      expect(
        matchesBarcodeFormat(BarcodeFormat.code128, [BarcodeFormat.gs1Code128]),
        isFalse,
      );
    },
  );
  test('UPC/EAN extension requires an actual add-on in the result', () {
    expect(
      matchesBarcodeFormat(BarcodeFormat.ean13, [
        BarcodeFormat.upcEanExtension,
      ], rawValue: '1234567890123 12345'),
      isTrue,
    );
    expect(
      matchesBarcodeFormat(BarcodeFormat.ean13, [
        BarcodeFormat.upcEanExtension,
      ], rawValue: '1234567890123'),
      isFalse,
    );
  });
}
