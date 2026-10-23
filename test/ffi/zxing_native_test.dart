@TestOn('vm')
library;

import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:mobile_scanner/mobile_scanner.dart';
import 'package:mobile_scanner/src/ffi/zxing_decoder.dart';
import 'package:mobile_scanner/src/ffi/zxing_image_analyzer.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  final enabled = Platform.environment['MS_ZXING_NATIVE_TESTS'] == '1';
  final manifest = (jsonDecode(
    File('test/fixtures/formats/manifest.json').readAsStringSync(),
  ) as List).cast<Map<String, dynamic>>();
  test('every concrete format in the pinned native header has a fixture', () {
    final header = File(
      'darwin/mobile_scanner/Sources/MobileScannerNative/zxing/BarcodeFormat.h',
    ).readAsStringSync();
    final names = RegExp(
      r'^\s*(\w+)\s*=\s*\(?1\s*<<',
      multiLine: true,
    ).allMatches(header).map((m) => m[1]!).toSet();
    expect(manifest.map((f) => f['native']).toSet(), names);
  });
  for (final fixture in manifest) {
    test(
      'native ${fixture['native']} decodes with explicit and all requests',
      () async {
        final format = BarcodeFormat.fromRawValue(fixture['raw'] as int);
        for (final requested in [
          <BarcodeFormat>[format],
          <BarcodeFormat>[],
        ]) {
          final results = await analyzeImageWithZxing(
            fixture['fixture'] as String,
            requested,
          );
          final result = results.singleWhere((b) => b.format == format);
          expect(result.rawValue, fixture['text']);
          expect(result.metadata?.formatName, fixture['formatName']);
        }
        expect(
          await analyzeImageWithZxing(fixture['fixture'] as String, [
            if (format == BarcodeFormat.qrCode)
              BarcodeFormat.rmQrCode
            else
              BarcodeFormat.qrCode,
          ]),
          isEmpty,
        );
      },
      skip: !enabled,
    );
  }
  test('combined masks 126/127 retain Code39 and Code128', () async {
    final formats = [
      BarcodeFormat.code39,
      BarcodeFormat.code93,
      BarcodeFormat.codabar,
      BarcodeFormat.dataMatrix,
      BarcodeFormat.ean13,
      BarcodeFormat.ean8,
    ];
    expect(
      await analyzeImageWithZxing('test/fixtures/formats/code39.png', formats),
      hasLength(1),
    );
    expect(
      await analyzeImageWithZxing('test/fixtures/formats/code128.png', [
        BarcodeFormat.code128,
        ...formats,
      ]),
      hasLength(1),
    );
  }, skip: !enabled);
  test('QR models and Unicode survive native metadata conversion', () async {
    final model1 = await analyzeImageWithZxing(
      'test/fixtures/formats/qrModel1.png',
      [BarcodeFormat.qrCode],
    );
    expect(model1.single.metadata?.formatName, 'QRCodeModel1');
    expect(model1.single.metadata?.symbologyIdentifier, ']Q0');
    final unicode = await analyzeImageWithZxing(
      'test/fixtures/qr_unicode.png',
      [],
    );
    expect(unicode.single.rawValue, 'Scanner ✓ 中文 😀');
    expect(unicode.single.metadata?.formatName, 'QRCodeModel2');
  }, skip: !enabled);
  test(
    'GS1-only, EAN/UPC family, DX family and add-on requests stay precise',
    () async {
      expect(
        await analyzeImageWithZxing('test/fixtures/formats/code128.png', [
          BarcodeFormat.gs1Code128,
        ]),
        isEmpty,
      );
      final gs1 = await analyzeImageWithZxing(
        'test/fixtures/formats/gs1Code128.png',
        [BarcodeFormat.gs1Code128],
      );
      expect(gs1.single.format, BarcodeFormat.gs1Code128);
      expect(gs1.single.metadata?.symbologyIdentifier, ']C1');
      expect(
        await analyzeImageWithZxing('test/fixtures/formats/ean13.png', [
          BarcodeFormat.eanUpc,
        ]),
        hasLength(1),
      );
      expect(
        await analyzeImageWithZxing('test/fixtures/formats/dxFilmEdge.png', [
          BarcodeFormat.otherBarcode,
        ]),
        hasLength(1),
      );
      final addon = await analyzeImageWithZxing(
        'test/fixtures/formats/eanAddOn.png',
        [BarcodeFormat.upcEanExtension],
      );
      expect(addon.single.rawValue, '9780735200449 51299');
    },
    skip: !enabled,
  );
  test('default multi-symbol decode is not capped at sixteen', () async {
    final results = await analyzeImageWithZxing(
      'test/fixtures/formats/twentyQr.png',
      [BarcodeFormat.qrCode],
    );
    expect(results, hasLength(20));
  }, skip: !enabled);
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
