import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mobile_scanner/mobile_scanner.dart';
import 'package:mobile_scanner_example/widgets/scanned_barcode_dialog.dart';

void main() {
  testWidgets('shows QR code content', (tester) async {
    const Barcode barcode = Barcode(
      displayValue: 'https://example.com',
      format: BarcodeFormat.qrCode,
    );

    await tester.pumpWidget(
      const MaterialApp(
        home: Scaffold(
          body: ScannedBarcodeDialog(barcode: barcode),
        ),
      ),
    );

    expect(find.text('QR code detected'), findsOneWidget);
    expect(find.text('https://example.com'), findsOneWidget);
  });
}
