# mobile_scanner

This fork combines its custom scanning engines and formats with upstream 7.4.2.
See [Upstream integration](UPSTREAM_INTEGRATION.md) for compatibility details
and the update workflow.

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
- **🎯 Universal Format Support**: 22+ barcode formats including Aztec, Data Matrix, PDF417, Code 11, MSI, Pharmacode
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

| Features     | Android            | iOS                | macOS              | Web |
|--------------|--------------------|--------------------|--------------------|-----|
| analyzeImage | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: | :x: |
| returnImage  | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: | :x: |
| scanWindow   | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: |
| autoZoom     | :heavy_check_mark: | :x:                | :x:                | :x: |
| lensType     | :heavy_check_mark: | :heavy_check_mark: | :x:                | :x: |
| getSupportedLenses(facing:) | :heavy_check_mark: | :heavy_check_mark: | :x: | :x: |
| getBestCloseRangeScanningLens | :heavy_check_mark: (always normal) | :heavy_check_mark: (requires iOS 15, falls back to normal) | :x: (always normal) | :x: (always normal) |

### Querying supported lens types with facing filter

Use `getSupportedLenses()` to query which lens types the device has. Pass an optional `facing` parameter to restrict results to cameras on one side:

```dart
// All cameras (back + front)
final Set<CameraLensType> allLenses = await controller.getSupportedLenses();

// Back cameras only
final Set<CameraLensType> backLenses = await controller.getSupportedLenses(
  facing: CameraFacing.back,
);

if (backLenses.contains(CameraLensType.zoom)) {
  // Device has a telephoto back camera
}
```

Without a facing filter, results may include lenses from both the front and back cameras, which can cause incorrect lens-type detection when switching cameras.

The `facing` filter is supported on Android and iOS. On macOS and the web, the filter is ignored and all lenses are returned.

### Automatic best-lens selection for close-range scanning

Use `getBestCloseRangeScanningLens()` to determine which lens can focus closest to the phone, and is therefore best suited for scanning barcodes held at close range. It returns `null` if the device has no camera for the given facing direction. Combine it with `getSupportedLenses` before switching, since the returned lens type is not guaranteed to be available:

```dart
final bestLens = await controller.getBestCloseRangeScanningLens(facing: CameraFacing.back);
final supported = await controller.getSupportedLenses(facing: CameraFacing.back);

if (bestLens != null && supported.contains(bestLens)) {
  await controller.switchCamera(
    SelectCamera(facingDirection: CameraFacing.back, lensType: bestLens),
  );
}
```

This returns:
- **iOS 15+**: The camera with the shortest `AVCaptureDevice.minimumFocusDistance` — typically the ultra-wide lens on newer iPhones (macro autofocus at ~2cm), or the standard 1x camera on older models
- **iOS < 15 / macOS**: `CameraLensType.normal`, since `minimumFocusDistance` is not available
- **Android**: `CameraLensType.normal` when the device has a camera. `LENS_INFO_MINIMUM_FOCUS_DISTANCE` is only meaningful on physical sub-cameras, which — like the rest of `getSupportedLenses` — are not independently selectable through CameraX, so the main camera (which has the most capable autofocus on virtually all Android devices) is returned unconditionally
- **Web**: `CameraLensType.normal` when the device has a camera. The MediaDevices API has no concept of lens type, and its `focusDistance` capability, where available at all, is limited to Chrome on Android
- **All platforms**: `null` if there is no camera for the requested facing direction

### Detection engine

mobile_scanner uses a hybrid pipeline. **ZXing-C++** is the primary decoder on
Android, Linux, and Windows; web selects the native BarcodeDetector API when
available for every requested format and otherwise uses ZXing-WASM, with
ZXing-js available as an option. CocoaPods and SwiftPM now compile the pinned
ZXing-C++ engine on iOS/macOS, with Apple Vision as the recovery decoder.
Apple builds require validation on Xcode; see [implementation status](IMPLEMENTATION_STATUS.md).

| Platform | Primary engine | ML fallback |
|----------|----------------|-------------|
| Android | ZXing-C++ (FFI/JNI) | ML Kit |
| iOS | ZXing-C++ (Apple build validation pending) | Apple Vision |
| macOS | ZXing-C++ (Apple build validation pending) | Apple Vision |
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
| | enableBatchProcessing (`analyzeImages`) | ✅ | ✅ | ✅ | ❌ |
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
| **DotCode** | 2D | ❌¹ | ❌¹ | ❌¹ | ❌¹ | ❌¹ |
| **Code 128** | 1D | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Code 39** | 1D | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Code 32 / Italian Pharmacode** | 1D | ✅⁵ | ⚠️²⁵ | ⚠️²⁵ | ❌⁵ | ✅⁵ |
| **EAN-13/8** | 1D | ✅ | ✅ | ✅ | ✅ | ✅ |
| **UPC-A/E** | 1D | ✅ | ✅ | ✅ | ✅ | ✅ |
| **ITF** | 1D | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Codabar** | 1D | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Code 93** | 1D | ✅ | ✅ | ✅ | ✅ | ✅ |
| **GS1 DataBar** | 1D | ✅ | ✅ | ✅ | ✅ | ✅ |
| **GS1 DataBar Expanded** | 1D | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Code 11** | 1D | ✅ | ⚠️³ | ⚠️³ | ❌³ | ✅ |
| **MSI / MSI Plessey** | 1D | ✅ | ⚠️³ | ⚠️³ | ❌³ | ✅ |
| **Pharmacode One-Track** | 1D | ✅⁴ | ⚠️³⁴ | ⚠️³⁴ | ❌³ | ✅⁴ |
| **Pharmacode Two-Track** | 1D | ✅⁴ | ⚠️³⁴ | ⚠️³⁴ | ❌³ | ✅⁴ |

¹ DotCode is reserved in the API but has no decoder in the pinned engine.
² The Apple native decoder is wired into CocoaPods and SwiftPM; Xcode and device
validation remain pending. Apple Vision also covers GS1 DataBar on supported OS versions.
³ Code 11, MSI/Plessey, and Pharmacode are decoded by mobile_scanner's native
linear fallback detector. They are not available through Android ML Kit, Apple
Vision, or ZXing-js web; on iOS/macOS they use the native ZXing-C++ bridge.
⁴ Pharmacode has no checksum, so the fallback detector only runs when
`BarcodeFormat.pharmaCode` or `BarcodeFormat.pharmaCodeTwoTrack` is explicitly
requested.
⁵ Code 32 is decoded as a distinct `BarcodeFormat.code32` by the native
ZXing-C++ engine when explicitly requested. Platform scanners may report the
same physical symbol as plain Code 39.

> **Not supported:** postal codes (PostNet / IMb / Planet etc.) are not
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

#### Detection backends

The web implementation supports three barcode detection backends. The active backend is selected
via `MobileScannerPlatform.instance.setWebBarcodeReader(reader)` before starting the scanner.

##### Auto (default)

Uses the native `BarcodeDetector` API when the browser supports it, and falls back to
`zxing-wasm` otherwise. This is the recommended setting for most apps.

##### Native BarcodeDetector

Uses the [W3C Shape Detection API](https://developer.mozilla.org/en-US/docs/Web/API/BarcodeDetector),
which is built into the browser. No external library is loaded. This is the fastest option because
detection runs natively without any JavaScript or WebAssembly overhead.

Available in Chrome 83+, Edge 83+, and Safari 17+. Not supported in Firefox.

##### zxing-wasm

Uses [zxing-wasm](https://github.com/Sec-ant/zxing-wasm), a WebAssembly port of the ZXing C++
library. The WASM binary (~2 MB) is loaded from the jsDelivr CDN on first use. Offers good
performance and works in all modern browsers, including Firefox.

##### ZXing-js (legacy)

Uses the [ZXing JavaScript library](https://github.com/zxing-js/library), a pure-JavaScript port
of ZXing. Loaded from the unpkg CDN. This backend is slower than the WASM alternative and is
provided for backward compatibility and comparison purposes only.

#### Backend comparison

| Feature                  | Native BarcodeDetector     | zxing-wasm                 | ZXing-js (legacy)          |
|--------------------------|----------------------------|----------------------------|----------------------------|
| **Performance**          | Fastest (native)           | Fast (WASM)                | Slow (pure JS)             |
| **Firefox**              | :x:                        | :heavy_check_mark:         | :heavy_check_mark:         |
| **Chrome / Edge**        | :heavy_check_mark:         | :heavy_check_mark:         | :heavy_check_mark:         |
| **Safari**               | 17+                        | :heavy_check_mark:         | :heavy_check_mark:         |
| **External dependency**  | None                       | ~2 MB WASM (CDN)           | ~600 KB JS (CDN)           |
| **Supported formats**    | Browser-dependent          | Most 1D and 2D formats     | Most 1D and 2D formats     |

#### Barcode format support per backend

| `BarcodeFormat`      | Native BarcodeDetector ¹ | zxing-wasm         | ZXing-js (legacy)  |
|----------------------|--------------------------|--------------------|--------------------|
| `aztec`              | :heavy_check_mark:       | :heavy_check_mark: | :heavy_check_mark: ³ |
| `codabar`            | :heavy_check_mark:       | :heavy_check_mark: | :heavy_check_mark: |
| `code39`             | :heavy_check_mark:       | :heavy_check_mark: | :heavy_check_mark: |
| `code93`             | :heavy_check_mark:       | :heavy_check_mark: | :heavy_check_mark: |
| `code128`            | :heavy_check_mark:       | :heavy_check_mark: | :heavy_check_mark: |
| `dataMatrix`         | :heavy_check_mark:       | :heavy_check_mark: | :heavy_check_mark: |
| `ean8`               | :heavy_check_mark:       | :heavy_check_mark: | :heavy_check_mark: |
| `ean13`              | :heavy_check_mark:       | :heavy_check_mark: | :heavy_check_mark: |
| `itf` ²              | :heavy_check_mark:       | :heavy_check_mark: | :heavy_check_mark: |
| `pdf417`             | :heavy_check_mark:       | :heavy_check_mark: | :heavy_check_mark: ³ |
| `qrCode`             | :heavy_check_mark:       | :heavy_check_mark: | :heavy_check_mark: |
| `upcA`               | :heavy_check_mark:       | :heavy_check_mark: | :heavy_check_mark: |
| `upcE`               | :heavy_check_mark:       | :heavy_check_mark: | :heavy_check_mark: |
| `maxiCode`           | :x:                      | :heavy_check_mark: | :heavy_check_mark: |
| `microQrCode`        | :x:                      | :heavy_check_mark: | :heavy_check_mark: |
| `dataBar` ⁴          | :x:                      | :heavy_check_mark: | :heavy_check_mark: |
| `dataBarExpanded` ⁴  | :x:                      | :heavy_check_mark: | :heavy_check_mark: |
| `dataBarLimited` ⁴   | :x:                      | :heavy_check_mark: | :x:                |

¹ The BarcodeDetector API defines all formats listed, but the formats that are actually
detected vary per browser and operating system. Use
[`BarcodeDetector.getSupportedFormats()`](https://developer.mozilla.org/en-US/docs/Web/API/BarcodeDetector/getSupportedFormats_static)
to query the current browser. In `WebBarcodeReader.auto` mode, mobile_scanner falls back to
zxing-wasm when the browser reports no supported formats at all.

² The `itf2of5` and `itf2of5WithChecksum` variants are decoded as plain ITF on all web
backends; no length or checksum validation is applied. The `itf14` variant is supported as a
distinct format by zxing-wasm, and decoded as plain ITF by the other backends.

³ The upstream [zxing-js library](https://github.com/zxing-js/library#supported-formats) marks
Aztec as "needs testing" and PDF417 as "not production ready".

⁴ GS1 DataBar (RSS-14) is also supported on iOS 15+ and macOS 12+ via Apple Vision.

zxing-wasm supports additional formats (such as rMQR Code), but only the formats listed above
are exposed through `BarcodeFormat`.

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

On Android, `rawBytes` is fully supported for all formats and encoding modes via MLKit.

On Web, support depends on the active detection backend:

| Backend                | `rawBytes` support                                      |
|------------------------|---------------------------------------------------------|
| Native BarcodeDetector | :x: Not available (browser API returns decoded text only) |
| zxing-wasm             | :heavy_check_mark: Fully supported                      |
| ZXing-js (legacy)      | :heavy_check_mark: Fully supported                      |

### Image batches and quality

```dart
final scanner = MobileScannerController(
  enableBatchProcessing: true,
  enableQualityAnalysis: true,
);
final results = await scanner.analyzeImages(
  imagePaths,
  maxConcurrent: 4,
);
```

Batches also work for image files on Linux and Windows. Results preserve input
order and use `null` for an image without a detection. Native image scanning runs
on worker isolates. On Android, live captures include `capture.imageQuality`
with normalized brightness, contrast and horizontal sharpness statistics. Quality
analysis does not require `returnImage`; it measures frames with detections.
Linux/Windows live camera capture is not implemented.
