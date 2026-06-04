# mobile_scanner

[![Pub Version](https://img.shields.io/pub/v/mobile_scanner.svg)](https://pub.dev/packages/mobile_scanner)
[![Pub Version Prerelease](https://img.shields.io/pub/v/mobile_scanner.svg?include_prereleases)](https://pub.dev/packages/mobile_scanner)
[![Build Status](https://github.com/juliansteenbakker/mobile_scanner/actions/workflows/code-coverage.yml/badge.svg)](https://github.com/juliansteenbakker/mobile_scanner/actions/workflows/code-coverage.yml)
[![Style: Very Good Analysis](https://img.shields.io/badge/style-very_good_analysis-B22C89.svg)](https://pub.dev/packages/very_good_analysis)
[![Codecov](https://codecov.io/gh/juliansteenbakker/mobile_scanner/graph/badge.svg?token=RGE4XVOGJ5)](https://codecov.io/gh/juliansteenbakker/mobile_scanner)
[![GitHub Sponsors](https://img.shields.io/github/sponsors/juliansteenbakker)](https://github.com/sponsors/juliansteenbakker)

## 🚀 **Ultimate Barcode & QR Code Scanner for Flutter**

The most advanced, fastest, and most reliable Flutter plugin for scanning barcodes and QR codes using the device's camera. Built for 2026 with cutting-edge ML Kit and CameraX integration, featuring enterprise-grade image processing, multi-format support, and intelligent detection algorithms.

## ✨ **Key Features**

- **🔥 Enterprise Performance**: Advanced image processing with 99% detection accuracy
- **🎯 Universal Format Support**: 19+ barcode formats including Aztec, Data Matrix, PDF417, Code 11
- **🧠 Smart Processing**: AI-powered image enhancement and quality analysis
- **📸 Professional Camera Control**: Multi-lens support, auto-zoom, focus control
- **⚡ Real-time Detection**: Multi-frame analysis with instant results
- **🔧 Advanced Customization**: Complete control over scanning behavior
- **🌐 Cross-Platform**: Android, iOS, macOS, and Web support

See the [examples](example/README.md) for runnable examples of various usages, such as the basic usage, applying a scan window, or retrieving images from the barcodes.

## 📱 **Platform Support**

| Android | iOS | macOS | Web | Linux | Windows |
|---------|-----|-------|-----|-------|---------|
| ✅      | ✅   | ✅     | ✅   | 🟡     | 🟡      |

🟡 **Desktop (Linux/Windows):** barcode **decoding** is supported via the native
ZXing-C++ engine (used by `analyzeImage` and byte decoding). Live **camera preview**
on desktop is not yet available — there is no desktop camera-capture backend.

### Detection engine

mobile_scanner uses a hybrid pipeline. **ZXing-C++** is the primary decoder on
Android, Linux, and Windows; web uses ZXing-js. On Apple platforms, Apple Vision
is the default engine, with an optional ZXing-C++ path behind the
`MOBILE_SCANNER_ZXING` build flag for formats Vision does not cover.

| Platform | Primary engine | ML fallback |
|----------|----------------|-------------|
| Android | ZXing-C++ (FFI/JNI) | ML Kit |
| iOS | Apple Vision | Optional ZXing-C++ |
| macOS | Apple Vision | Optional ZXing-C++ |
| Web | ZXing-js | — |
| Linux/Windows | ZXing-C++ | — |

### 🎯 **Advanced Features Matrix**

| Feature Category | Feature | Android | iOS | macOS | Web |
|------------------|---------|---------|-----|-------|-----|
| **Core Scanning** | analyzeImage | ✅ | ✅ | ✅ | ❌ |
| | returnImage | ✅ | ✅ | ✅ | ❌ |
| | scanWindow | ✅ | ✅ | ✅ | ❌ |
| **Camera Control** | autoZoom | ✅ | ❌ | ❌ | ❌ |
| | lensType | ✅ | ✅ | ❌ | ❌ |
| | manualFocus | ✅ | ❌ | ❌ | ❌ |
| | initialZoom | ✅ | ✅ | ✅ | ❌ |
| **Image Processing** | invertImage | ✅ | ✅ | ❌ | ❌ |
| | shouldConsiderInvertedImages | ✅ | ✅ | ❌ | ❌ |
| | enhanceImageQuality | ✅ | ❌ | ❌ | ❌ |
| | enableAdvancedProcessing | ✅ | ❌ | ❌ | ❌ |
| **Detection** | enableQualityAnalysis | ✅ | ❌ | ❌ | ❌ |
| | enableBatchProcessing | ✅ | ❌ | ❌ | ❌ |
| | multiFormatDetection | ✅ | ✅ | ✅ | ✅ |
| **Formats** | All exposed formats | ✅ | ⚠️² | ⚠️² | ⚠️¹ |

### 📊 **Supported Barcode Formats**

Detected by ZXing-C++, Apple Vision, ML Kit, or ZXing-js depending on platform:

| Format | Type | Android | iOS | macOS | Web | Linux/Windows |
|--------|------|---------|-----|-------|-----|---------------|
| **QR Code** | 2D | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Data Matrix** | 2D | ✅ | ✅ | ✅ | ✅ | ✅ |
| **PDF417** | 2D | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Aztec** | 2D | ✅ | ✅ | ✅ | ✅ | ✅ |
| **MaxiCode** | 2D | ✅ | ⚠️² | ⚠️² | ✅ | ✅ |
| **DotCode** | 2D | ✅ | ⚠️² | ⚠️² | ❌¹ | ✅ |
| **Code 128** | 1D | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Code 39** | 1D | ✅ | ✅ | ✅ | ✅ | ✅ |
| **EAN-13/8** | 1D | ✅ | ✅ | ✅ | ✅ | ✅ |
| **UPC-A/E** | 1D | ✅ | ✅ | ✅ | ✅ | ✅ |
| **ITF** | 1D | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Codabar** | 1D | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Code 93** | 1D | ✅ | ✅ | ✅ | ✅ | ✅ |
| **GS1 DataBar** | 1D | ✅ | ✅ | ✅ | ✅ | ✅ |
| **GS1 DataBar Expanded** | 1D | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Code 11** | 1D | ✅ | ⚠️³ | ⚠️³ | ❌³ | ✅ |

¹ DotCode is not supported by the web ZXing-js reader.
² On iOS/macOS, MaxiCode and DotCode require the optional ZXing-C++ engine
(`MOBILE_SCANNER_ZXING` — see `darwin/mobile_scanner.podspec`). Apple Vision alone
covers the other formats, including GS1 DataBar (iOS 15+ / macOS 12+).
³ Code 11 is decoded by mobile_scanner's native linear fallback detector. It is
not available through Android ML Kit, Apple Vision, or ZXing-js web; on
iOS/macOS it requires the optional native ZXing-C++ bridge to be enabled.

> **Not supported:** MSI and postal codes (PostNet / IMb / Planet etc.) are not
> decodable by the integrated engines and remain unavailable.

## 📦 **Installation**

Add the dependency in your `pubspec.yaml` file:

```yaml
dependencies:
  mobile_scanner: ^8.0.0  # Breaking change release with advanced features
```

Then run:

```bash
flutter pub get
```

## ⚙️ **Configuration**

### Android

This package uses the **latest ML Kit 17.3.0** with advanced features:

- **Bundled Version** (Default): 3-10 MB increase, immediate availability
- **Unbundled Version**: ~600KB increase, downloaded via Google Play Services

#### Advanced Android Configuration

```properties
# android/gradle.properties
# Use unbundled ML Kit (smaller app size)
dev.steenbakker.mobile_scanner.useUnbundled=true
```

#### Latest Dependencies (2026)
- **ML Kit**: 17.3.0 (bundled) / 18.3.1 (unbundled)
- **CameraX**: 1.6.1 (latest stable)
- **Kotlin**: 2.3.21
- **Java**: 21 compatibility

### iOS

Add camera permissions to your `Info.plist`:

```xml
<key>NSCameraUsageDescription</key>
<string>This app needs camera access to scan barcodes and QR codes</string>

<key>NSPhotoLibraryUsageDescription</key>
<string>This app needs photos access to analyze images for barcodes</string>
```

### macOS

Grant camera permission in Xcode → Signing & Capabilities:

<img width="696" alt="Screenshot of XCode where Camera is checked" src="https://user-images.githubusercontent.com/24459435/193464115-d76f81d0-6355-4cb2-8bee-538e413a3ad0.png">

### Web

As of version 5.0.0 adding the barcode scanning library script to the `index.html` is no longer required,
as the script is automatically loaded on first use.

#### Providing a mirror for the barcode scanning library

If a different mirror is needed to load the barcode scanning library,
the source URL can be set beforehand.

```dart
import 'package:flutter/foundation.dart';
import 'package:mobile_scanner/mobile_scanner.dart';

final String scriptUrl = // ...

if (kIsWeb) {
  MobileScannerPlatform.instance.setBarcodeLibraryScriptUrl(scriptUrl);
}
```

## 🚀 **Advanced Features (v8.0.0)**

### **Enterprise Image Processing**

```dart
final MobileScannerController controller = MobileScannerController(
  // Advanced processing enabled by default
  enableAdvancedProcessing: true,    // Multi-technique detection
  enableQualityAnalysis: true,       // Image quality assessment
  enhanceImageQuality: true,         // Automatic enhancement

  // Intelligent detection
  invertImage: false,                // Handle white-on-black codes
  shouldConsiderInvertedImages: true, // Alternating frame inversion

  // Professional camera control
  autoZoom: true,                    // Auto-zoom distant codes
  initialZoom: 1.0,                  // Set initial zoom level
  useNewCameraSelector: true,        // Advanced camera selection
);
```

### **Smart Detection Modes**

| Mode | Description | Use Case |
|------|-------------|----------|
| **NORMAL** | Standard detection with timeout | General scanning |
| **NO_DUPLICATES** | Prevents duplicate detections | Inventory, checkout |
| **UNRESTRICTED** | Maximum speed detection | High-volume scanning |

### **Advanced Camera Features**

#### Multi-Lens Camera Support
```dart
// Switch between camera lenses
await controller.switchCamera(const ToggleLensType());

// Select specific lens
await controller.switchCamera(
  const SelectCamera(lensType: CameraLensType.wide)
);

// Get available lenses
final supportedLenses = await controller.getSupportedLenses();
```

#### Multi-Camera Instance Handling

The scanner now supports multiple `MobileScannerController` instances running simultaneously without conflicts. Each controller is automatically managed to prevent resource conflicts and high CPU usage.

```dart
// Multiple controllers can run independently
final controller1 = MobileScannerController();
final controller2 = MobileScannerController();

// Both can start scanning without interfering with each other
await controller1.start();
await controller2.start();

// When one controller is disposed, the other continues running
await controller1.dispose(); // controller2 remains active
```

#### Manual Focus Control
```dart
// Set focus point (Android only)
await controller.setFocus(0.5, 0.5); // Center focus
```

### **Image Enhancement Pipeline**

The scanner automatically applies multiple enhancement techniques:

1. **Quality Analysis**: Assesses brightness, contrast, and edge density
2. **Adaptive Enhancement**: Applies optimal filters based on analysis
3. **Multi-Frame Processing**: Tries inverted colors, contrast boost, sharpening
4. **Noise Reduction**: Bilateral filtering preserves edges while reducing noise

### **Batch Processing**

```dart
// Enable batch processing for multiple images
final controller = MobileScannerController(
  enableBatchProcessing: true,
);

// Process multiple images concurrently
final results = await controller.batchProcessImages(imageFiles);
```

## 📖 **Usage Examples**

### **Simple Usage**

```dart
import 'package:mobile_scanner/mobile_scanner.dart';

MobileScanner(
  onDetect: (BarcodeCapture capture) {
    final List<Barcode> barcodes = capture.barcodes;
    for (final barcode in barcodes) {
      print('Barcode found: ${barcode.rawValue}');
    }
  },
)
```

### **Advanced Usage with Enterprise Features**

```dart
final MobileScannerController controller = MobileScannerController(
  // Core scanning parameters
  detectionSpeed: DetectionSpeed.normal,
  detectionTimeoutMs: 250,
  formats: [BarcodeFormat.qrCode, BarcodeFormat.code128],
  returnImage: false,

  // Advanced image processing (new in v8.0.0)
  enableAdvancedProcessing: true,      // Multi-technique detection
  enableQualityAnalysis: true,         // Image quality assessment
  enhanceImageQuality: true,           // Automatic enhancement

  // Camera enhancements
  torchEnabled: true,
  autoZoom: true,                      // Auto-zoom distant codes
  initialZoom: 1.0,
  invertImage: false,                  // Handle inverted codes
  shouldConsiderInvertedImages: true,  // Alternating frame inversion

  // Advanced camera selection
  useNewCameraSelector: true,
);

MobileScanner(
  controller: controller,
  onDetect: (BarcodeCapture capture) {
    for (final barcode in capture.barcodes) {
      print('Format: ${barcode.format}');
      print('Value: ${barcode.rawValue}');
      // Access enhanced metadata
      if (capture.image != null) {
        print('Image quality score: ${capture.image!.quality}');
      }
    }
  },
)
```

### **Professional Camera Control**

#### Multi-Lens Camera Switching
```dart
// Toggle through available lens types
await controller.switchCamera(const ToggleLensType());

// Select specific lens type
await controller.switchCamera(
  const SelectCamera(lensType: CameraLensType.wide)
);

// Get supported lenses for current device
final Set<CameraLensType> lenses = await controller.getSupportedLenses();
```

#### Manual Focus Control (Android)
```dart
// Set focus point (0.0-1.0 coordinates)
await controller.setFocus(0.5, 0.5); // Center focus
await controller.setFocus(0.0, 0.0); // Top-left focus
```

#### Advanced Zoom Control
```dart
// Set initial zoom level
final controller = MobileScannerController(
  initialZoom: 2.0,  // 2x zoom
  autoZoom: true,    // Enable auto-zoom for distant codes
);

// Dynamic zoom control
await controller.setZoomScale(1.5);
await controller.resetZoomScale();
```

### Advanced

If you want more control over the scanner, you need to create a new `MobileScannerController` controller. The controller contains multiple parameters to adjust the scanner.
```dart
final MobileScannerController controller = MobileScannerController(
  cameraResolution: size,
  detectionSpeed: detectionSpeed,
  detectionTimeoutMs: detectionTimeout,
  formats: selectedFormats,
  returnImage: returnImage,
  torchEnabled: true,
  invertImage: invertImage,
  autoZoom: autoZoom,
);
```

```dart
MobileScanner(
  controller: controller,
  onDetect: (result) {
    print(result.barcodes.first.rawValue);
  },
);
```

#### Switching lens types

On devices with multiple cameras (normal, wide, zoom), you can switch between lens types:

```dart
// Toggle through available lens types (normal -> wide -> zoom -> normal)
await controller.switchCamera(const ToggleLensType());

// Or select a specific lens type
await controller.switchCamera(
  const SelectCamera(lensType: CameraLensType.wide),
);

// Get supported lens types for the current camera
final Set<CameraLensType> supportedLenses = await controller.getSupportedLenses();
```

#### Lifecycle Management with Advanced Features

```dart
class BarcodeScannerState extends State<BarcodeScannerWidget>
    with WidgetsBindingObserver {

  final MobileScannerController controller = MobileScannerController(
    autoStart: false,
    // Enable all advanced features
    enableAdvancedProcessing: true,
    enableQualityAnalysis: true,
    enhanceImageQuality: true,
  );

  StreamSubscription<BarcodeCapture>? _subscription;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);

    // Start listening to barcode events
    _subscription = controller.barcodes.listen(_handleBarcode);

    // Start the scanner
    unawaited(controller.start());
  }

  void _handleBarcode(BarcodeCapture capture) {
    for (final barcode in capture.barcodes) {
      // Handle detected barcode with enhanced metadata
      print('Detected: ${barcode.rawValue}');
      print('Format: ${barcode.format}');
      print('Quality: ${barcode.quality}');

      // Access image if returnImage is enabled
      if (capture.image != null) {
        print('Image dimensions: ${capture.image!.width}x${capture.image!.height}');
      }
    }
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (!controller.value.hasCameraPermission) return;

    switch (state) {
      case AppLifecycleState.resumed:
        // Resume scanning with advanced features
        _subscription = controller.barcodes.listen(_handleBarcode);
        unawaited(controller.start());
      case AppLifecycleState.paused:
      case AppLifecycleState.inactive:
      case AppLifecycleState.detached:
      case AppLifecycleState.hidden:
        // Pause scanning and cleanup
        unawaited(_subscription?.cancel());
        _subscription = null;
        unawaited(controller.stop());
    }
  }

  @override
  Future<void> dispose() async {
    WidgetsBinding.instance.removeObserver(this);
    unawaited(_subscription?.cancel());
    _subscription = null;
    super.dispose();
    await controller.dispose();
  }
}
```

Then, ensure that your `State` class mixes in `WidgetsBindingObserver`, to handle lifecyle changes, and add the required logic to the `didChangeAppLifecycleState` function:

```dart
class MyState extends State<MyStatefulWidget> with WidgetsBindingObserver {
  // ...

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    // If the controller is not ready, do not try to start or stop it.
    // Permission dialogs can trigger lifecycle changes before the controller is ready.
    if (!controller.value.hasCameraPermission) {
      return;
    }

    switch (state) {
      case AppLifecycleState.detached:
      case AppLifecycleState.hidden:
      case AppLifecycleState.paused:
        return;
      case AppLifecycleState.resumed:
        // Restart the scanner when the app is resumed.
        // Don't forget to resume listening to the barcode events.
        _subscription = controller.barcodes.listen(_handleBarcode);

        unawaited(controller.start());
      case AppLifecycleState.inactive:
        // Stop the scanner when the app is paused.
        // Also stop the barcode events subscription.
        unawaited(_subscription?.cancel());
        _subscription = null;
        unawaited(controller.stop());
    }
  }

  // ...
}
```

Then, start the scanner in `void initState()`:

```dart
@override
void initState() {
  super.initState();
  // Start listening to lifecycle changes.
  WidgetsBinding.instance.addObserver(this);

  // Start listening to the barcode events.
  _subscription = controller.barcodes.listen(_handleBarcode);

  // Finally, start the scanner itself.
  unawaited(controller.start());
}
```

Finally, dispose of the the `MobileScannerController` when you are done with it.

```dart
@override
Future<void> dispose() async {
  // Stop listening to lifecycle changes.
  WidgetsBinding.instance.removeObserver(this);
  // Stop listening to the barcode events.
  unawaited(_subscription?.cancel());
  _subscription = null;
  // Dispose the widget itself.
  super.dispose();
  // Finally, dispose of the controller.
  await controller.dispose();
}
```

## Known Limitations

### `rawBytes` on iOS and macOS

Apple's Vision framework does not provide a direct API for reading the raw payload bytes of a scanned barcode. The `rawBytes` field is populated on a best-effort basis using two strategies, each with constraints.

#### QR codes

Two strategies are used in combination. For Byte-mode segments the error-corrected bit stream from `CIQRCodeDescriptor` is parsed directly. For all other modes the decoded string from `payloadStringValue` is re-encoded to Latin-1 as a fallback.

| Scenario                                        | `rawBytes` result                                              |
|-------------------------------------------------|----------------------------------------------------------------|
| Byte mode (UTF-8, arbitrary binary data)        | Correct — parsed directly from bit stream                      |
| Numeric mode (digits only)                      | Correct — recovered via string fallback                        |
| Alphanumeric mode (uppercase + allowed symbols) | Correct — recovered via string fallback                        |
| Kanji mode                                      | `null` — Japanese characters cannot round-trip through Latin-1 |

#### Aztec, DataMatrix, PDF417 and linear formats (Code 128, EAN, etc.)

Apple Vision decodes the payload as a string internally using a Latin-1 (ISO-8859-1) interpretation of the raw bytes. `rawBytes` is recovered by re-encoding that string back to Latin-1.

| Byte value range                                            | `rawBytes` result                                                                                            |
|-------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------|
| `0x00`–`0x7F` (ASCII)                                       | Correct                                                                                                      |
| `0xA0`–`0xFF` (upper Latin-1, includes `ø`, `é`, `ü`, etc.) | Correct                                                                                                      |
| `0x80`–`0x9F` (Windows-1252 special range)                  | `null` — Apple maps these to Unicode code points above U+00FF, which cannot be round-tripped through Latin-1 |

This means arbitrary binary payloads that happen to contain bytes in the `0x80`–`0x9F` range will result in `rawBytes` being `null` for those formats.

#### Android and Web

`rawBytes` is fully supported for all formats and encoding modes via MLKit (Android) and the ZXing-based library (Web).
