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
  test(
    'real rMQR fixtures retain their format and honor explicit format filters',
    () async {
      for (final name in [
        'rmqr',
        'rmqr_large',
        'rmqr_inverted',
        'rmqr_rotated',
      ]) {
        final captures = await analyzeImageWithZxing(
          'test/fixtures/$name.png',
          [BarcodeFormat.rmQrCode],
        );
        expect(captures, hasLength(1), reason: name);
        expect(captures.single.format, BarcodeFormat.rmQrCode);
        expect(captures.single.rawValue, isNotEmpty);
        expect(captures.single.corners, hasLength(4));
        final all = await analyzeImageWithZxing('test/fixtures/$name.png', []);
        expect(all.single.format, BarcodeFormat.rmQrCode);
        expect(
          await analyzeImageWithZxing('test/fixtures/$name.png', [
            BarcodeFormat.code11,
          ]),
          isEmpty,
        );
        expect(
          await analyzeImageWithZxing('test/fixtures/$name.png', [
            BarcodeFormat.qrCode,
          ]),
          isEmpty,
        );
      }
    },
    skip: !enabled,
  );
}
