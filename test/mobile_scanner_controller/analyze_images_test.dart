import 'package:flutter_test/flutter_test.dart';
import 'package:mobile_scanner/mobile_scanner.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  late MobileScannerPlatform original;
  setUp(() => original = MobileScannerPlatform.instance);
  tearDown(() => MobileScannerPlatform.instance = original);

  test(
    'batches are sequential by default and preserve empty results',
    () async {
      final fake = _BatchPlatform();
      MobileScannerPlatform.instance = fake;
      final controller = MobileScannerController(autoStart: false);
      final results = await controller.analyzeImages(['slow', 'empty', 'fast']);
      expect(fake.peak, 1);
      expect(results.map((c) => c?.barcodes.single.rawValue), [
        'slow',
        null,
        'fast',
      ]);
      await controller.dispose();
    },
  );
  test(
    'opt-in batches use bounded parallel workers and preserve input order',
    () async {
      final fake = _BatchPlatform();
      MobileScannerPlatform.instance = fake;
      final controller = MobileScannerController(
        autoStart: false,
        enableBatchProcessing: true,
      );
      final results = await controller.analyzeImages([
        'slow',
        'fast',
        'empty',
        'last',
      ], maxConcurrent: 2);
      expect(fake.peak, 2);
      expect(results.map((c) => c?.barcodes.single.rawValue), [
        'slow',
        'fast',
        null,
        'last',
      ]);
      expect(controller.analyzeImages([], maxConcurrent: 0), throwsRangeError);
      await controller.dispose();
    },
  );
  test(
    'waits for in-flight work when a file fails and rejects disposed use',
    () async {
      final fake = _BatchPlatform();
      MobileScannerPlatform.instance = fake;
      final controller = MobileScannerController(
        autoStart: false,
        enableBatchProcessing: true,
      );
      await expectLater(
        controller.analyzeImages(['error', 'slow']),
        throwsStateError,
      );
      expect(fake.active, 0);
      await controller.dispose();
      await expectLater(
        controller.analyzeImages(['fast']),
        throwsA(isA<MobileScannerException>()),
      );
    },
  );
}

class _BatchPlatform extends MobileScannerPlatform {
  int active = 0;
  int peak = 0;
  @override
  Future<BarcodeCapture?> analyzeImage(
    String path, {
    List<BarcodeFormat> formats = const [],
  }) async {
    active++;
    if (active > peak) peak = active;
    try {
      await Future<void>.delayed(
        Duration(milliseconds: path == 'slow' ? 30 : 1),
      );
      if (path == 'error') throw StateError('Unreadable image');
      if (path == 'empty') return null;
      return BarcodeCapture(barcodes: [Barcode(rawValue: path)]);
    } finally {
      active--;
    }
  }

  @override
  Future<void> dispose(int id) async {}
}
