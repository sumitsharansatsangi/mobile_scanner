import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mobile_scanner/src/enums/barcode_format.dart';
import 'package:mobile_scanner/src/enums/camera_facing.dart';
import 'package:mobile_scanner/src/enums/camera_lens_type.dart';
import 'package:mobile_scanner/src/enums/detection_speed.dart';
import 'package:mobile_scanner/src/objects/start_options.dart';

void main() {
  group('StartOptions tests', () {
    test('toMap includes all required fields', () {
      const options = StartOptions(
        cameraDirection: CameraFacing.back,
        cameraLensType: CameraLensType.normal,
        cameraResolution: null,
        detectionSpeed: DetectionSpeed.normal,
        detectionTimeoutMs: 250,
        formats: [],
        returnImage: false,
        torchEnabled: false,
        useNewCameraSelector: false,
        shouldConsiderInvertedImages: false,
        invertImage: false,
        autoZoom: false,
        initialZoom: null,
      );

      final map = options.toMap();

      expect(map, {
        'facing': CameraFacing.back.rawValue,
        'lensType': CameraLensType.normal.rawValue,
        'speed': DetectionSpeed.normal.rawValue,
        'timeout': 250,
        'returnImage': false,
        'torch': false,
        'useNewCameraSelector': false,
        'shouldConsiderInvertedImages': false,
        'invertImage': false,
        'autoZoom': false,
        'initialZoom': null,
        'enableAdvancedProcessing': true,
        'enableQualityAnalysis': false,
        'enableBatchProcessing': false,
        'enhanceImageQuality': true,
      });
    });

    test('toMap includes camera resolution when provided', () {
      const options = StartOptions(
        cameraDirection: CameraFacing.back,
        cameraLensType: CameraLensType.any,
        cameraResolution: Size(1920, 1080),
        detectionSpeed: DetectionSpeed.normal,
        detectionTimeoutMs: 250,
        formats: [],
        returnImage: false,
        torchEnabled: false,
        useNewCameraSelector: false,
        shouldConsiderInvertedImages: false,
        invertImage: false,
        autoZoom: false,
        initialZoom: null,
      );

      final map = options.toMap();

      expect(map['cameraResolution'], [1920, 1080]);
    });

    test('toMap excludes camera resolution when null', () {
      const options = StartOptions(
        cameraDirection: CameraFacing.back,
        cameraLensType: CameraLensType.any,
        cameraResolution: null,
        detectionSpeed: DetectionSpeed.normal,
        detectionTimeoutMs: 250,
        formats: [],
        returnImage: false,
        torchEnabled: false,
        useNewCameraSelector: false,
        shouldConsiderInvertedImages: false,
        invertImage: false,
        autoZoom: false,
        initialZoom: null,
      );

      final map = options.toMap();

      expect(map.containsKey('cameraResolution'), false);
    });

    test('toMap includes formats when provided', () {
      const options = StartOptions(
        cameraDirection: CameraFacing.back,
        cameraLensType: CameraLensType.any,
        cameraResolution: null,
        detectionSpeed: DetectionSpeed.normal,
        detectionTimeoutMs: 250,
        formats: [BarcodeFormat.qrCode, BarcodeFormat.code128],
        returnImage: false,
        torchEnabled: false,
        useNewCameraSelector: false,
        shouldConsiderInvertedImages: false,
        invertImage: false,
        autoZoom: false,
        initialZoom: null,
      );

      final map = options.toMap();

      expect(map['formats'], [
        BarcodeFormat.qrCode.rawValue,
        BarcodeFormat.code128.rawValue,
      ]);
    });

    test('toMap includes Code 39 and QR Code formats together', () {
      const options = StartOptions(
        cameraDirection: CameraFacing.back,
        cameraLensType: CameraLensType.any,
        cameraResolution: null,
        detectionSpeed: DetectionSpeed.normal,
        detectionTimeoutMs: 250,
        formats: [BarcodeFormat.code39, BarcodeFormat.qrCode],
        returnImage: false,
        torchEnabled: false,
        useNewCameraSelector: false,
        shouldConsiderInvertedImages: false,
        invertImage: false,
        autoZoom: false,
        initialZoom: null,
      );

      final map = options.toMap();

      expect(map['formats'], [
        BarcodeFormat.code39.rawValue,
        BarcodeFormat.qrCode.rawValue,
      ]);
    });

    test(
      'toMap includes Pharmacode Two-Track format when explicitly provided',
      () {
        const options = StartOptions(
          cameraDirection: CameraFacing.back,
          cameraLensType: CameraLensType.any,
          cameraResolution: null,
          detectionSpeed: DetectionSpeed.normal,
          detectionTimeoutMs: 250,
          formats: [BarcodeFormat.pharmaCodeTwoTrack],
          returnImage: false,
          torchEnabled: false,
          useNewCameraSelector: false,
          shouldConsiderInvertedImages: false,
          invertImage: false,
          autoZoom: false,
          initialZoom: null,
        );

        final map = options.toMap();

        expect(map['formats'], [BarcodeFormat.pharmaCodeTwoTrack.rawValue]);
      },
    );

    test(
      'toMap includes UPC/EAN extension format when explicitly provided',
      () {
        const options = StartOptions(
          cameraDirection: CameraFacing.back,
          cameraLensType: CameraLensType.any,
          cameraResolution: null,
          detectionSpeed: DetectionSpeed.normal,
          detectionTimeoutMs: 250,
          formats: [BarcodeFormat.upcEanExtension],
          returnImage: false,
          torchEnabled: false,
          useNewCameraSelector: false,
          shouldConsiderInvertedImages: false,
          invertImage: false,
          autoZoom: false,
          initialZoom: null,
        );

        final map = options.toMap();

        expect(map['formats'], [BarcodeFormat.upcEanExtension.rawValue]);
      },
    );

    test('toMap includes GS1-128 format when explicitly provided', () {
      const options = StartOptions(
        cameraDirection: CameraFacing.back,
        cameraLensType: CameraLensType.any,
        cameraResolution: null,
        detectionSpeed: DetectionSpeed.normal,
        detectionTimeoutMs: 250,
        formats: [BarcodeFormat.gs1Code128],
        returnImage: false,
        torchEnabled: false,
        useNewCameraSelector: false,
        shouldConsiderInvertedImages: false,
        invertImage: false,
        autoZoom: false,
        initialZoom: null,
      );

      final map = options.toMap();

      expect(map['formats'], [BarcodeFormat.gs1Code128.rawValue]);
    });

    test('toMap excludes formats when empty', () {
      const options = StartOptions(
        cameraDirection: CameraFacing.back,
        cameraLensType: CameraLensType.any,
        cameraResolution: null,
        detectionSpeed: DetectionSpeed.normal,
        detectionTimeoutMs: 250,
        formats: [],
        returnImage: false,
        torchEnabled: false,
        useNewCameraSelector: false,
        shouldConsiderInvertedImages: false,
        invertImage: false,
        autoZoom: false,
        initialZoom: null,
      );

      final map = options.toMap();

      expect(map.containsKey('formats'), false);
    });

    test('toMap correctly maps all lens types', () {
      for (final lensType in CameraLensType.values) {
        final options = StartOptions(
          cameraDirection: CameraFacing.back,
          cameraLensType: lensType,
          cameraResolution: null,
          detectionSpeed: DetectionSpeed.normal,
          detectionTimeoutMs: 250,
          formats: [],
          returnImage: false,
          torchEnabled: false,
          useNewCameraSelector: false,
          shouldConsiderInvertedImages: false,
          invertImage: false,
          autoZoom: false,
          initialZoom: null,
        );

        final map = options.toMap();

        expect(
          map['lensType'],
          lensType.rawValue,
          reason:
              'Lens type $lensType should map to raw value '
              '${lensType.rawValue}',
        );
      }
    });

    test('toMap includes initialZoom when provided', () {
      const options = StartOptions(
        cameraDirection: CameraFacing.back,
        cameraLensType: CameraLensType.any,
        cameraResolution: null,
        detectionSpeed: DetectionSpeed.normal,
        detectionTimeoutMs: 250,
        formats: [],
        returnImage: false,
        torchEnabled: false,
        useNewCameraSelector: false,
        shouldConsiderInvertedImages: false,
        invertImage: false,
        autoZoom: false,
        initialZoom: 2,
      );

      final map = options.toMap();

      expect(map['initialZoom'], 2);
    });

    test('toMap handles all camera facing directions', () {
      for (final facing in CameraFacing.values) {
        final options = StartOptions(
          cameraDirection: facing,
          cameraLensType: CameraLensType.any,
          cameraResolution: null,
          detectionSpeed: DetectionSpeed.normal,
          detectionTimeoutMs: 250,
          formats: [],
          returnImage: false,
          torchEnabled: false,
          useNewCameraSelector: false,
          shouldConsiderInvertedImages: false,
          invertImage: false,
          autoZoom: false,
          initialZoom: null,
        );

        final map = options.toMap();

        expect(
          map['facing'],
          facing.rawValue,
          reason:
              'Camera facing $facing should map to raw value '
              '${facing.rawValue}',
        );
      }
    });

    test('toMap handles boolean flags correctly', () {
      // Test representative combinations rather than all 16 permutations
      const testCases = [
        (returnImage: true, torch: true, invertImage: true, autoZoom: true),
        (returnImage: false, torch: false, invertImage: false, autoZoom: false),
        (returnImage: true, torch: false, invertImage: true, autoZoom: false),
      ];

      for (final testCase in testCases) {
        final options = StartOptions(
          cameraDirection: CameraFacing.back,
          cameraLensType: CameraLensType.any,
          cameraResolution: null,
          detectionSpeed: DetectionSpeed.normal,
          detectionTimeoutMs: 250,
          formats: [],
          returnImage: testCase.returnImage,
          torchEnabled: testCase.torch,
          useNewCameraSelector: false,
          shouldConsiderInvertedImages: false,
          invertImage: testCase.invertImage,
          autoZoom: testCase.autoZoom,
          initialZoom: null,
        );

        final map = options.toMap();

        expect(map['returnImage'], testCase.returnImage);
        expect(map['torch'], testCase.torch);
        expect(map['invertImage'], testCase.invertImage);
        expect(map['autoZoom'], testCase.autoZoom);
        expect(map['useNewCameraSelector'], false);
        expect(map['shouldConsiderInvertedImages'], false);
        expect(map['enableAdvancedProcessing'], true);
        expect(map['enableQualityAnalysis'], false);
        expect(map['enableBatchProcessing'], false);
        expect(map['enhanceImageQuality'], true);
      }
    });

    test('toMap includes advanced processing options', () {
      const options = StartOptions(
        cameraDirection: CameraFacing.back,
        cameraLensType: CameraLensType.any,
        cameraResolution: null,
        detectionSpeed: DetectionSpeed.normal,
        detectionTimeoutMs: 250,
        formats: [],
        returnImage: false,
        torchEnabled: false,
        useNewCameraSelector: true,
        shouldConsiderInvertedImages: true,
        invertImage: true,
        autoZoom: true,
        initialZoom: 1.5,
        enableAdvancedProcessing: false,
        enableQualityAnalysis: true,
        enableBatchProcessing: true,
        enhanceImageQuality: false,
      );

      final map = options.toMap();

      expect(map['useNewCameraSelector'], true);
      expect(map['shouldConsiderInvertedImages'], true);
      expect(map['invertImage'], true);
      expect(map['autoZoom'], true);
      expect(map['initialZoom'], 1.5);
      expect(map['enableAdvancedProcessing'], false);
      expect(map['enableQualityAnalysis'], true);
      expect(map['enableBatchProcessing'], true);
      expect(map['enhanceImageQuality'], false);
    });
  });
}
