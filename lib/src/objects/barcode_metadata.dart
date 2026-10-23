/// Details supplied by the decoder in addition to the public format family.
///
/// For example, [formatName] distinguishes QRCodeModel1 from QRCodeModel2,
/// or MicroPDF417 from PDF417, when the selected engine identifies them.
class BarcodeMetadata {
  /// Creates optional decoder metadata.
  const BarcodeMetadata({
    this.formatName,
    this.symbologyIdentifier,
    this.version,
    this.errorCorrectionLevel,
    this.isInverted,
    this.isMirrored,
    this.sequenceSize,
    this.sequenceIndex,
    this.sequenceId,
  });

  /// Creates metadata from a platform decoder result.
  factory BarcodeMetadata.fromNative(Map<Object?, Object?> data) {
    String? string(String key) {
      final value = data[key] as String?;
      return value == null || value.isEmpty ? null : value;
    }

    int? sequence(String key) {
      final value = (data[key] as num?)?.toInt();
      return value == null || value < 0 ? null : value;
    }

    return BarcodeMetadata(
      formatName: string('formatName'),
      symbologyIdentifier: string('symbologyIdentifier'),
      version: string('version'),
      errorCorrectionLevel: string('errorCorrectionLevel'),
      isInverted: data['isInverted'] as bool?,
      isMirrored: data['isMirrored'] as bool?,
      sequenceSize: sequence('sequenceSize'),
      sequenceIndex: sequence('sequenceIndex'),
      sequenceId: string('sequenceId'),
    );
  }

  /// The engine's exact format or variant name.
  final String? formatName;

  /// AIM symbology identifier, such as `]Q1` or `]C1`.
  final String? symbologyIdentifier;

  /// Symbol version or dimensions, according to the decoder.
  final String? version;

  /// Error correction level, where applicable.
  final String? errorCorrectionLevel;

  /// Whether this result was decoded with reversed reflectance.
  final bool? isInverted;

  /// Whether this result was decoded as a mirrored symbol.
  final bool? isMirrored;

  /// Number of symbols in a structured-append sequence; zero means unknown.
  final int? sequenceSize;

  /// Zero-based symbol index within a structured-append sequence.
  final int? sequenceIndex;

  /// Sequence identifier used to associate structured-append symbols.
  final String? sequenceId;
}
