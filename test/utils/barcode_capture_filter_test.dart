import 'package:flutter_test/flutter_test.dart';
import 'package:mobile_scanner/mobile_scanner.dart';
import 'package:mobile_scanner/src/utils/barcode_capture_filter.dart';

void main() {
  test(
    'recovery reader results are filtered without losing capture metadata',
    () {
      const quality = ImageQuality(
        brightness: 0.1,
        contrast: 0.1,
        sharpness: 0,
      );
      const capture = BarcodeCapture(
        barcodes: [
          Barcode(format: BarcodeFormat.qrCode, rawValue: 'QR'),
          Barcode(format: BarcodeFormat.ean13, rawValue: '1234567890123'),
        ],
        raw: 'native',
        imageQuality: quality,
      );
      final filtered = filterBarcodeCapture(capture, [BarcodeFormat.ean13])!;
      expect(filtered.barcodes.single.format, BarcodeFormat.ean13);
      expect(filtered.imageQuality, same(quality));
      expect(filtered.imageQuality?.isDark, isTrue);
      expect(filtered.raw, 'native');
      expect(filterBarcodeCapture(capture, [BarcodeFormat.dotCode]), isNull);
    },
  );
}
