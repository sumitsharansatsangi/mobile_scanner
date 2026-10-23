@TestOn('browser')
library;

import 'dart:js_interop';
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:mobile_scanner/mobile_scanner.dart';
import 'package:mobile_scanner/src/web/barcode_detector/barcode_detector_reader.dart';
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
