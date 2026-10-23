@TestOn('vm')
library;

import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';
import 'dart:ui' as ui;

import 'package:flutter_test/flutter_test.dart';
import 'package:mobile_scanner/mobile_scanner.dart';
import 'package:mobile_scanner/src/ffi/ms_zxing_bindings.dart';
import 'package:mobile_scanner/src/ffi/zxing_decoder.dart';
import 'package:mobile_scanner/src/ffi/zxing_image_analyzer.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  final enabled = Platform.environment['MS_ZXING_NATIVE_TESTS'] == '1';
  const cases = [
    ('postnet5', '12345', BarcodeFormat.postnet),
    ('postnet9', '123456789', BarcodeFormat.postnet),
    ('postnet11', '12345678901', BarcodeFormat.postnet),
    ('planet11', '40123456789', BarcodeFormat.planet),
    ('planet13', '4012345678901', BarcodeFormat.planet),
  ];
  for (final (name, text, format) in cases) {
    test('native $name checks format, checksum, bytes and corners', () async {
      for (final suffix in ['', '_rotated', '_inverted', '_upside_down']) {
        final file = 'test/fixtures/postal/$name$suffix.png';
        for (final formats in [
          [format],
          <BarcodeFormat>[],
          [BarcodeFormat.postnet, BarcodeFormat.planet, BarcodeFormat.qrCode],
        ]) {
          final results = await analyzeImageWithZxing(file, formats);
          final postal = results.where((b) => b.format == format).toList();
          expect(postal, hasLength(1), reason: '$name$suffix $formats');
          final barcode = postal.single;
          expect(barcode.rawValue, text, reason: '$name$suffix');
          expect(
            (barcode.rawDecodedBytes! as DecodedBarcodeBytes).bytes,
            utf8.encode(text),
          );
          expect(
            barcode.metadata?.formatName,
            format == BarcodeFormat.postnet ? 'POSTNET' : 'PLANET',
          );
          expect(barcode.metadata?.isInverted, suffix == '_inverted');
          expect(barcode.corners, hasLength(4));
          expect(barcode.size.width, greaterThan(2));
          expect(barcode.size.height, greaterThan(2));
        }
      }
      expect(
        await analyzeImageWithZxing(
          'test/fixtures/postal/${name}_bad_checksum.png',
          [format],
        ),
        isEmpty,
      );
      expect(
        await analyzeImageWithZxing('test/fixtures/postal/$name.png', [
          if (format == BarcodeFormat.postnet)
            BarcodeFormat.planet
          else
            BarcodeFormat.postnet,
        ]),
        isEmpty,
      );
      expect(
        await analyzeImageWithZxing('test/fixtures/postal/$name.png', [
          BarcodeFormat.qrCode,
        ]),
        isEmpty,
      );
    }, skip: !enabled);
  }
  test('postal requests cannot widen into QR or DX family requests', () async {
    for (final file in [
      'test/fixtures/mecard.png',
      'test/fixtures/formats/dxFilmEdge.png',
    ]) {
      expect(
        await analyzeImageWithZxing(file, [
          BarcodeFormat.postnet,
          BarcodeFormat.planet,
        ]),
        isEmpty,
      );
    }
  }, skip: !enabled);
  test(
    'postal raw decoding honors stride, color, crop and scan options',
    () async {
      for (final (name, text, format) in [cases.first, cases[3]]) {
        final bytes = await File('test/fixtures/postal/$name.png')
            .readAsBytes();
        final codec = await ui.instantiateImageCodec(bytes);
        final frame = await codec.getNextFrame();
        final image = frame.image;
        final rgba = (await image.toByteData())!.buffer.asUint8List();
        final stride = image.width * 4 + 20;
        final padded = Uint8List(stride * image.height)
          ..fillRange(0, stride * image.height, 255);
        for (var y = 0; y < image.height; y++) {
          padded.setRange(
            y * stride,
            y * stride + image.width * 4,
            rgba,
            y * image.width * 4,
          );
        }
        final decoder = ZxingDecoder.instance;
        final found = decoder.decode(
          padded,
          width: image.width,
          height: image.height,
          rowStride: stride,
          pixelFormat: MsZxingPixelFormat.rgba,
          formats: [format],
          crop: ui.Rect.fromLTWH(16, 16, image.width - 32, 64),
          tryHarder: true,
        );
        expect(found.single.rawValue, text);
        expect(found.single.corners.first, const ui.Offset(24, 24));
        expect(found.single.corners[3].dy - found.single.corners[0].dy, 48);
        expect(
          decoder.decode(
            padded,
            width: image.width,
            height: image.height,
            rowStride: stride,
            pixelFormat: MsZxingPixelFormat.bgra,
            formats: [format],
            crop: const ui.Rect.fromLTWH(0, 0, 16, 16),
          ),
          isEmpty,
        );
        final inverted = Uint8List.fromList(rgba.map((v) => 255 - v).toList());
        expect(
          decoder.decode(
            inverted,
            width: image.width,
            height: image.height,
            pixelFormat: MsZxingPixelFormat.rgba,
            formats: [format],
          ),
          isEmpty,
        );
        final rotated = await File('test/fixtures/postal/${name}_rotated.png')
            .readAsBytes();
        final rotatedCodec = await ui.instantiateImageCodec(rotated);
        final rotatedFrame = await rotatedCodec.getNextFrame();
        final rotatedImage = rotatedFrame.image;
        final rotatedRgba = (await rotatedImage.toByteData())!.buffer
            .asUint8List();
        expect(
          decoder.decode(
            rotatedRgba,
            width: rotatedImage.width,
            height: rotatedImage.height,
            pixelFormat: MsZxingPixelFormat.rgba,
            formats: [format],
          ),
          isEmpty,
        );
        image.dispose();
        codec.dispose();
        rotatedImage.dispose();
        rotatedCodec.dispose();
      }
    },
    skip: !enabled,
  );
  test('postal guards and legal lengths are mandatory', () async {
    for (final name in [
      'postnet_bad_length',
      'planet_bad_length',
      'postnet5_missing_guard',
      'planet11_missing_guard',
    ]) {
      expect(
        await analyzeImageWithZxing('test/fixtures/postal/$name.png', [
          BarcodeFormat.postnet,
          BarcodeFormat.planet,
        ]),
        isEmpty,
        reason: name,
      );
    }
  }, skip: !enabled);
}
