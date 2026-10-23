@TestOn('vm')
library;

import 'dart:io';
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:mobile_scanner/mobile_scanner.dart';
import 'package:mobile_scanner/src/ffi/zxing_decoder.dart';
import 'package:mobile_scanner/src/ffi/zxing_image_analyzer.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  final enabled = Platform.environment['MS_ZXING_NATIVE_TESTS'] == '1';
  test('real native file decode retains parsed MECARD data', () async {
    final result = await analyzeImageWithZxing('test/fixtures/mecard.png', [
      BarcodeFormat.qrCode,
    ]);
    expect(result, hasLength(1));
    expect(result.single.type, BarcodeType.contactInfo);
    expect(result.single.contactInfo?.name, isNotNull);
    expect(result.single.contactInfo?.emails, isNotEmpty);
    expect(result.single.rawDecodedBytes, isA<DecodedBarcodeBytes>());
    expect(result.single.size.width, 240);
    expect(
      await analyzeImageWithZxing('test/fixtures/mecard.png', [
        BarcodeFormat.dotCode,
      ]),
      isEmpty,
    );
  }, skip: !enabled);

  test('native decoder rejects short buffers before accessing memory', () {
    expect(
      () => ZxingDecoder.instance.decode(Uint8List(1), width: 100, height: 100),
      throwsArgumentError,
    );
    expect(
      ZxingDecoder.instance.decode(
        Uint8List(4),
        width: 2,
        height: 2,
        formats: [BarcodeFormat.unknown],
      ),
      isEmpty,
    );
  }, skip: !enabled);
}
