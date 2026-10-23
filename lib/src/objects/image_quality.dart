/// Measurements of the input frame, each normalized to the range 0–1.
///
/// These are image statistics, not a probability that decoding will succeed.
/// Currently supplied by Android when quality analysis is enabled.
class ImageQuality {
  /// Creates image quality statistics.
  const ImageQuality({
    required this.brightness,
    required this.contrast,
    required this.sharpness,
  });

  /// Creates statistics received from the native frame analyzer.
  factory ImageQuality.fromNative(Map<Object?, Object?> data) => ImageQuality(
    brightness: (data['brightness']! as num).toDouble(),
    contrast: (data['contrast']! as num).toDouble(),
    sharpness: (data['sharpness']! as num).toDouble(),
  );

  /// Mean luminance, from black (0) to white (1).
  final double brightness;

  /// Luminance standard deviation relative to maximum contrast.
  final double contrast;

  /// Mean horizontal neighboring pixel difference relative to 255.
  final double sharpness;

  /// Whether the frame has low mean luminance.
  bool get isDark => brightness < 0.2;

  /// Whether the frame has little variation in luminance.
  bool get hasLowContrast => contrast < 0.15;
}
