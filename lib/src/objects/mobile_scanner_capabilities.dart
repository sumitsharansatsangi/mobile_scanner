/// @docImport 'package:mobile_scanner/src/mobile_scanner_controller.dart';
library;

/// The optional controls that the active camera supports.
///
/// Use these flags to show or hide controls in your UI. A flag is only `true`
/// when the camera really provides that control.
class MobileScannerCapabilities {
  /// Create a new [MobileScannerCapabilities] instance.
  const new({required this.torch, required this.zoom, required this.focus});

  /// No optional controls are supported.
  ///
  /// This is the value before the camera has started.
  const new none() : torch = false, zoom = false, focus = false;

  /// Whether the flashlight can be switched on and off with
  /// [MobileScannerController.toggleTorch].
  final bool torch;

  /// Whether [MobileScannerController.setZoomScale] has an effect.
  ///
  /// On desktop this is digital zoom, a crop of the camera image.
  final bool zoom;

  /// Whether [MobileScannerController.setFocusPoint] has an effect.
  final bool focus;

  @override
  String toString() {
    return 'MobileScannerCapabilities('
        'torch: $torch, zoom: $zoom, focus: $focus)';
  }
}
