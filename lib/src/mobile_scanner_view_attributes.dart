import 'package:flutter/services.dart';
import 'package:mobile_scanner/src/enums/camera_facing.dart';
import 'package:mobile_scanner/src/enums/torch_state.dart';
import 'package:mobile_scanner/src/objects/mobile_scanner_capabilities.dart';

/// This class defines the attributes for the mobile scanner view.
class MobileScannerViewAttributes {
  /// Construct a new [MobileScannerViewAttributes] instance.
  const new({
    required this.cameraDirection,
    required this.currentTorchMode,
    required this.size,
    this.numberOfCameras,
    this.initialDeviceOrientation,
    this.capabilities = const MobileScannerCapabilities.none(),
  });

  /// The direction of the active camera.
  final CameraFacing cameraDirection;

  /// The current torch state of the active camera.
  final TorchState currentTorchMode;

  /// The number of available cameras.
  final int? numberOfCameras;

  /// The size of the camera output.
  final Size size;

  /// The initial orientation of the device.
  final DeviceOrientation? initialDeviceOrientation;

  /// The optional controls that the active camera supports.
  final MobileScannerCapabilities capabilities;
}
