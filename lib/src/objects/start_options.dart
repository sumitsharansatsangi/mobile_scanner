import 'dart:ui';

import 'package:mobile_scanner/src/enums/barcode_format.dart';
import 'package:mobile_scanner/src/enums/camera_facing.dart';
import 'package:mobile_scanner/src/enums/camera_lens_type.dart';
import 'package:mobile_scanner/src/enums/detection_speed.dart';

/// This class defines the different start options for the mobile scanner.
class StartOptions {
  /// Construct a new [StartOptions] instance.
  const StartOptions({
    required this.cameraDirection,
    required this.cameraLensType,
    required this.cameraResolution,
    required this.detectionSpeed,
    required this.detectionTimeoutMs,
    required this.formats,
    required this.returnImage,
    required this.torchEnabled,
    required this.useNewCameraSelector,
    required this.shouldConsiderInvertedImages,
    required this.invertImage,
    required this.autoZoom,
    required this.initialZoom,
    this.enableAdvancedProcessing = true,
    this.enableQualityAnalysis = false,
    this.enableBatchProcessing = false,
    this.enhanceImageQuality = true,
  });

  /// The direction for the camera.
  final CameraFacing cameraDirection;

  /// The desired camera resolution for the scanner.
  final Size? cameraResolution;

  /// The preferred lens type for the camera.
  ///
  /// This allows selection between normal, wide, and zoom lenses on devices
  /// with multiple cameras.
  ///
  /// When set to [CameraLensType.any], the first available camera for the
  /// given [cameraDirection] will be used.
  final CameraLensType cameraLensType;

  /// Invert image colors for analyzer to support white-on-black barcodes, which
  /// are not supported by MLKit.
  final bool invertImage;

  /// The detection speed for the scanner.
  final DetectionSpeed detectionSpeed;
 
  /// Whether the scanner should try to detect color-inverted barcodes in every other frame.
  final bool shouldConsiderInvertedImages;

  /// The detection timeout for the scanner, in milliseconds.
  final int detectionTimeoutMs;

  /// The barcode formats to detect.
  final List<BarcodeFormat> formats;

  /// Whether the detected barcodes should provide their image data.
  final bool returnImage;

  /// Whether the torch should be turned on when the scanner starts.
  final bool torchEnabled;

  /// Whether the camera should auto zoom if the detected code is to far from
  /// the camera.
  ///
  /// This option is only supported on Android. Other platforms will ignore this
  /// option.
  final bool autoZoom;

  /// Whether to use the new camera selector.
  ///
  /// This option is only supported on Android.
  final bool useNewCameraSelector;



  /// The initial zoom scale factor for the camera.
  ///
  /// Currently only supported on iOS, MacOS and Android.
  final double? initialZoom;

  /// Enable advanced barcode processing with multiple enhancement techniques.
  ///
  /// When enabled, the scanner will try multiple image processing techniques
  /// for better barcode detection. Defaults to true in breaking change release.
  final bool enableAdvancedProcessing;

  /// Enable image quality analysis for barcode detection optimization.
  ///
  /// When enabled, the scanner will analyze image quality and suggest enhancements.
  /// Defaults to false.
  final bool enableQualityAnalysis;

  /// Enable batch processing for multiple images.
  ///
  /// When enabled, the scanner can process multiple images concurrently.
  /// Defaults to false.
  final bool enableBatchProcessing;

  /// Enable automatic image quality enhancement.
  ///
  /// When enabled, the scanner will automatically enhance image quality for better detection.
  /// Defaults to true.
  final bool enhanceImageQuality;

  /// Converts this object to a map.
  Map<String, Object?> toMap() {
    return <String, Object?>{
      if (cameraResolution != null)
        'cameraResolution': <int>[
          cameraResolution!.width.toInt(),
          cameraResolution!.height.toInt(),
        ],
      'facing': cameraDirection.rawValue,
      'lensType': cameraLensType.rawValue,
      if (formats.isNotEmpty)
        'formats': formats.map((f) => f.rawValue).toList(),
      'returnImage': returnImage,
      'speed': detectionSpeed.rawValue,
      'timeout': detectionTimeoutMs,
      'torch': torchEnabled,
      'useNewCameraSelector': useNewCameraSelector,
      'shouldConsiderInvertedImages': shouldConsiderInvertedImages,
      'invertImage': invertImage,
      'autoZoom': autoZoom,
      'initialZoom': initialZoom,
      'enableAdvancedProcessing': enableAdvancedProcessing,
      'enableQualityAnalysis': enableQualityAnalysis,
      'enableBatchProcessing': enableBatchProcessing,
      'enhanceImageQuality': enhanceImageQuality,
    };
  }
}
