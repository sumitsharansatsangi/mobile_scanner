@TestOn('browser')
library;

import 'dart:js_interop';
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:mobile_scanner/mobile_scanner.dart';
import 'package:mobile_scanner/src/web/barcode_detector/barcode_detector_reader.dart';
import 'package:mobile_scanner/src/web/barcode_reader_factory.dart';
import 'package:mobile_scanner/src/web/javascript_map.dart';
import 'package:mobile_scanner/src/web/zxing/result.dart' as legacy;
import 'package:mobile_scanner/src/web/zxing_wasm/zxing_wasm_barcode_reader.dart';
import 'package:mobile_scanner/src/web/zxing_wasm/zxing_wasm_result.dart';
import 'package:web/web.dart' as web;

@JS('eval')
external JSAny? _evaluate(JSString script);

@JS('detectorCalls')
external int get _detectorCalls;

void main() {
  setUp(() {
    _evaluate(
      """
      globalThis.detectorCalls = 0;
      globalThis.BarcodeDetector = class {
        static getSupportedFormats() { return Promise.resolve(['qr_code', 'ean_13']); }
        constructor(options) { globalThis.detectorCalls++; }
        detect(video) { return Promise.resolve([]); }
      };
    """
          .toJS,
    );
  });
  test(
    'auto all-formats uses WASM; explicit supported formats use native',
    () async {
      for (final formats in [
        <BarcodeFormat>[],
        [BarcodeFormat.all],
        [BarcodeFormat.rmQrCode],
        [BarcodeFormat.telepen],
      ]) {
        final selected = await createWebBarcodeReader(
          WebBarcodeReader.auto,
          formats,
        );
        expect(selected.backend, WebBarcodeReader.zxingWasm);
      }
      expect(
        (await createWebBarcodeReader(WebBarcodeReader.auto, [
          BarcodeFormat.qrCode,
        ])).backend,
        WebBarcodeReader.barcodeDetector,
      );
    },
  );
  test('WASM variants and GS1 retain their identities and metadata', () {
    for (final (variant, expected) in [
      ('QRCodeModel1', BarcodeFormat.qrCode),
      ('MicroPDF417', BarcodeFormat.pdf417),
      ('AztecRune', BarcodeFormat.aztec),
      ('PZN', BarcodeFormat.pzn),
      ('Code32', BarcodeFormat.code32),
      ('TelepenNumeric', BarcodeFormat.telepen),
      ('DXFilmEdge', BarcodeFormat.dxFilmEdge),
      ('Code128', BarcodeFormat.gs1Code128),
    ]) {
      final result = ZXingWasmReadResult(
        {
              'format': variant,
              'text': 'test',
              'symbologyIdentifier': ']C1',
              'version': '1',
              'ecLevel': 'L',
              'isInverted': true,
              'isMirrored': false,
              'sequenceSize': -1,
              'sequenceIndex': -1,
              'sequenceId': '',
              'position': <String, Object?>{},
            }.jsify()!
            as JSObject,
      ).toBarcode;
      expect(result.format, expected);
      expect(result.metadata?.formatName, variant);
      expect(result.metadata?.isInverted, isTrue);
      expect(result.metadata?.sequenceIndex, isNull);
    }
  });
  test('WASM EAN supplements retain a separator and Model1 stays distinct', () {
    final result = ZXingWasmReadResult(
      {
            'format': 'EAN13',
            'text': '978073520044951299',
            'extra': '{"EanAddOn":"51299"}',
            'position': <String, Object?>{},
          }.jsify()!
          as JSObject,
    ).toBarcode;
    expect(result.rawValue, '9780735200449 51299');
    final model1 = ZXingWasmReadResult(
      {
            'format': 'QRCode',
            'text': 'QR Code Model 1 ',
            'symbologyIdentifier': ']Q0',
            'position': <String, Object?>{},
          }.jsify()!
          as JSObject,
    ).toBarcode;
    expect(model1.metadata?.formatName, 'QRCodeModel1');
  });
  test('legacy GS1 and EAN supplement conversion preserves identity', () {
    final gs1 = legacy.Result(
      {'barcodeFormat': 4, 'text': ']C1168901'}.jsify()! as JSObject,
    ).toBarcode;
    expect(gs1.format, BarcodeFormat.gs1Code128);
    expect(gs1.rawValue, '168901');
    final metadata = JSMap<JSNumber, JSAny>()..set(7.toJS, '51299'.toJS);
    final object =
        {
              'barcodeFormat': 7,
              'text': '9780735200449',
              'resultMetadata': metadata,
            }.jsify()!
            as JSObject;
    expect(legacy.Result(object).toBarcode.rawValue, '9780735200449 51299');
  });
  test('native auto selection requires every requested format', () async {
    expect(
      await BarcodeDetectorReader.isSupported([BarcodeFormat.qrCode]),
      isTrue,
    );
    expect(
      await BarcodeDetectorReader.isSupported([
        BarcodeFormat.qrCode,
        BarcodeFormat.maxiCode,
      ]),
      isFalse,
    );
    expect(
      await BarcodeDetectorReader.isSupported([BarcodeFormat.unknown]),
      isFalse,
    );
  });
  test(
    'unsupported-only native requests never construct an unrestricted detector',
    () async {
      final reader = BarcodeDetectorReader();
      await reader.prepareDecoder(
        _options(formats: [BarcodeFormat.dotCode]),
      );
      expect(await reader.decodeFrame(web.HTMLVideoElement()), isEmpty);
      expect(_detectorCalls, 0);
      await reader.prepareDecoder(
        _options(formats: [BarcodeFormat.unknown]),
      );
      expect(await reader.decodeFrame(web.HTMLVideoElement()), isEmpty);
      expect(_detectorCalls, 0);
    },
  );
  test(
    'unsupported-only WASM requests do not load or invoke the WASM module',
    () async {
      final reader = ZXingWasmBarcodeReader();
      await reader.prepareDecoder(
        _options(formats: [BarcodeFormat.dotCode]),
      );
      expect(await reader.decodeFrame(web.HTMLVideoElement()), isEmpty);
      await reader.prepareDecoder(
        _options(formats: [BarcodeFormat.unknown]),
      );
      expect(await reader.decodeFrame(web.HTMLVideoElement()), isEmpty);
      reader.disposeDecoder();
    },
  );
  test('WASM payload conversion retains typed data and parses WiFi', () {
    final result = ZXingWasmReadResult(
      {
            'text': 'WIFI:T:WPA;S:Test;P:secret;;',
            'format': 'QRCode',
            'bytes': Uint8List.fromList([1, 2, 3]),
            'isValid': true,
            'position': {
              'topLeft': {'x': 0, 'y': 0},
              'topRight': {'x': 10, 'y': 0},
              'bottomRight': {'x': 10, 'y': 10},
              'bottomLeft': {'x': 0, 'y': 10},
            },
          }.jsify()!
          as JSObject,
    );
    final barcode = result.toBarcode;
    expect(barcode.type, BarcodeType.wifi);
    expect(barcode.wifi?.ssid, 'Test');
    expect(barcode.corners, hasLength(4));
  });
}

StartOptions _options({required List<BarcodeFormat> formats}) => StartOptions(
  cameraDirection: CameraFacing.back,
  cameraLensType: CameraLensType.any,
  cameraResolution: null,
  detectionSpeed: DetectionSpeed.normal,
  detectionTimeoutMs: 250,
  formats: formats,
  returnImage: false,
  torchEnabled: false,
  useNewCameraSelector: false,
  shouldConsiderInvertedImages: false,
  invertImage: false,
  autoZoom: false,
  initialZoom: null,
);
