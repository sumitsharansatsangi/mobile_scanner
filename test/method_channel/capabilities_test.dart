import 'package:flutter/foundation.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mobile_scanner/mobile_scanner.dart';
import 'package:mobile_scanner/src/method_channel/mobile_scanner_method_channel.dart';

const _options = StartOptions(
  cameraDirection: CameraFacing.back,
  cameraLensType: CameraLensType.any,
  cameraResolution: null,
  detectionSpeed: DetectionSpeed.normal,
  detectionTimeoutMs: 250,
  formats: <BarcodeFormat>[],
  returnImage: false,
  torchEnabled: false,
  useNewCameraSelector: false,
  shouldConsiderInvertedImages: false,
  invertImage: false,
  autoZoom: false,
  initialZoom: null,
);

void expectCapabilities(
  MobileScannerCapabilities capabilities, {
  required bool torch,
  required bool zoom,
  required bool focus,
}) {
  expect(capabilities.torch, torch);
  expect(capabilities.zoom, zoom);
  expect(capabilities.focus, focus);
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  group('start reports capabilities', () {
    late MethodChannelMobileScanner platform;

    setUp(() {
      // A desktop target avoids the Android surface-producer start path.
      debugDefaultTargetPlatformOverride = TargetPlatform.linux;
      platform = MethodChannelMobileScanner();
      MobileScannerPlatform.instance = platform;
    });

    tearDown(() {
      debugDefaultTargetPlatformOverride = null;
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .setMockMethodCallHandler(platform.methodChannel, null);
    });

    void mockStart(Map<String, Object?> startResult) {
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .setMockMethodCallHandler(platform.methodChannel, (methodCall) async {
            if (methodCall.method == 'request') return true;
            if (methodCall.method == 'start') return startResult;
            return null;
          });
    }

    Future<MobileScannerViewAttributes> startWith() {
      return platform.start(
        1,
        _options,
        startRequest: () async {},
        stopRequest: () async {},
      );
    }

    test('uses the capabilities the native side reports', () async {
      mockStart({
        'textureId': 1,
        'currentTorchState': 0,
        'capabilities': {'torch': true, 'zoom': true, 'focus': false},
      });

      final attributes = await startWith();

      expectCapabilities(
        attributes.capabilities,
        torch: true,
        zoom: true,
        focus: false,
      );
    });

    test('a camera without torch reports torch as unavailable', () async {
      mockStart({
        'textureId': 1,
        'currentTorchState': -1,
        'capabilities': {'torch': false, 'zoom': true, 'focus': false},
      });

      final attributes = await startWith();

      expect(attributes.capabilities.torch, isFalse);
      expect(attributes.currentTorchMode, TorchState.unavailable);
    });

    test('falls back to the start result when nothing is reported', () async {
      debugDefaultTargetPlatformOverride = TargetPlatform.iOS;
      mockStart({'textureId': 1, 'currentTorchState': 1});

      final attributes = await startWith();

      expectCapabilities(
        attributes.capabilities,
        torch: true,
        zoom: true,
        focus: true,
      );
    });

    test('fallback on desktop does not offer focus', () async {
      debugDefaultTargetPlatformOverride = TargetPlatform.linux;
      mockStart({'textureId': 1, 'currentTorchState': -1});

      final attributes = await startWith();

      expect(attributes.capabilities.torch, isFalse);
      expect(attributes.capabilities.focus, isFalse);
      expect(attributes.capabilities.zoom, isTrue);
    });
  });

  test('MobileScannerState starts with no capabilities', () {
    const state = MobileScannerState.uninitialized();
    expectCapabilities(
      state.capabilities,
      torch: false,
      zoom: false,
      focus: false,
    );
  });
}
