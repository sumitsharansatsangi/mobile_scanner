import 'package:flutter/material.dart';
import 'package:mobile_scanner/mobile_scanner.dart';

/// Shows the content of a detected barcode.
class ScannedBarcodeDialog extends StatelessWidget {
  /// Construct a new [ScannedBarcodeDialog] instance.
  const ScannedBarcodeDialog({required this.barcode, super.key});

  /// The barcode to display.
  final Barcode barcode;

  List<int>? get _bytes {
    final BarcodeBytes? rawDecodedBytes = barcode.rawDecodedBytes;

    return switch (rawDecodedBytes) {
      DecodedBarcodeBytes() => rawDecodedBytes.bytes,
      DecodedVisionBarcodeBytes() =>
        rawDecodedBytes.bytes ?? rawDecodedBytes.rawBytes,
      null => null,
    };
  }

  String get _content {
    final String? value = barcode.displayValue ?? barcode.rawValue;

    if (value != null && value.isNotEmpty) {
      return value;
    }

    final List<int>? bytes = _bytes;

    if (bytes != null && bytes.isNotEmpty) {
      return bytes.join(', ');
    }

    return 'No readable content was found.';
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: Text(
        barcode.format == BarcodeFormat.qrCode
            ? 'QR code detected'
            : 'Barcode detected',
      ),
      content: SingleChildScrollView(
        child: SelectableText(_content),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.of(context).pop(),
          child: const Text('Close'),
        ),
      ],
    );
  }
}
