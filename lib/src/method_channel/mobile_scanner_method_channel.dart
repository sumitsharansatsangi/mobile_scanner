import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';
import 'package:mobile_scanner/src/enums/barcode_format.dart';
import 'package:mobile_scanner/src/enums/camera_facing.dart';
import 'package:mobile_scanner/src/enums/camera_lens_type.dart';
import 'package:mobile_scanner/src/enums/mobile_scanner_authorization_state.dart';
import 'package:mobile_scanner/src/enums/mobile_scanner_error_code.dart';
import 'package:mobile_scanner/src/enums/torch_state.dart';
import 'package:mobile_scanner/src/ffi/zxing_image_analyzer.dart';
import 'package:mobile_scanner/src/method_channel/android_surface_producer_delegate.dart';
import 'package:mobile_scanner/src/method_channel/rotated_preview.dart';
import 'package:mobile_scanner/src/mobile_scanner_exception.dart';
import 'package:mobile_scanner/src/mobile_scanner_platform_interface.dart';
import 'package:mobile_scanner/src/mobile_scanner_view_attributes.dart';
import 'package:mobile_scanner/src/objects/barcode.dart';
import 'package:mobile_scanner/src/objects/barcode_capture.dart';
import 'package:mobile_scanner/src/objects/start_options.dart';
import 'package:mobile_scanner/src/utils/parse_device_orientation_extension.dart';

/// An implementation of [MobileScannerPlatform] that uses method channels.
class MethodChannelMobileScanner extends MobileScannerPlatform {
  /// The name of the barcode event that is sent when a barcode is scanned.
  @visibleForTesting
  static const String kBarcodeEventName = 'barcode';

  /// The name of the error event that is sent when a barcode scan error occurs.
  @visibleForTesting
  static const String kBarcodeErrorEventName = 'MOBILE_SCANNER_BARCODE_ERROR';

  /// The name of the error event that is sent when an operation is not
  /// supported.
  @visibleForTesting
  static const String kUnsupportdOperationErrorEventName =
      'MOBILE_SCANNER_UNSUPPORTED_OPERATION';

  /// The name of the torch state event.
  @visibleForTesting
  static const String kTorchStateEventName = 'torchState';

  /// The name of the zoom scale state event.
  @visibleForTesting
  static const String kZoomScaleStateEventName = 'zoomScaleState';

  /// The name of the method that gets the camera authorization state.
  @visibleForTesting
  static const String kAuthorizationStateMethodName = 'state';

  /// The name of the method that requests camera permissions.
  @visibleForTesting
  static const String kRequestAuthorizationMethodName = 'request';

  /// The name of the method that analyzes an image for barcodes.
  @visibleForTesting
  static const String kAnalyzeImageMethodName = 'analyzeImage';

  /// The name of the method that resets the zoom scale.
  @visibleForTesting
  static const String kResetScaleMethodName = 'resetScale';

  /// The name of the method that sets the zoom scale.
  @visibleForTesting
  static const String kSetScaleMethodName = 'setScale';

  /// The name of the method that sets the focus point.
  @visibleForTesting
  static const String kSetFocusMethodName = 'setFocus';

  /// The name of the method that starts the camera.
  @visibleForTesting
  static const String kStartCameraMethodName = 'start';

  /// The name of the method that stops the camera.
  @visibleForTesting
  static const String kStopCameraMethodName = 'stop';

  /// The name of the method that pauses the camera.
  @visibleForTesting
  static const String kPauseCameraMethodName = 'pause';

  /// The name of the method that toggles the torch.
  @visibleForTesting
  static const String kToggleTorchMethodName = 'toggleTorch';

  /// The name of the method that updates the scan window.
  @visibleForTesting
  static const String kUpdateScanWindowMethodName = 'updateScanWindow';

  /// The name of the method that gets the supported camera lenses.
  @visibleForTesting
  static const String kGetSupportedLensesMethodName = 'getSupportedLenses';

  /// The method channel used to interact with the native platform.
  @visibleForTesting
  final methodChannel = const MethodChannel(
    'dev.steenbakker.mobile_scanner/scanner/method',
  );

  /// The event channel that sends back device orientation change events.
  @visibleForTesting
  final deviceOrientationEventChannel = const EventChannel(
    'dev.steenbakker.mobile_scanner/scanner/deviceOrientation',
  );

  /// The event channel that sends back scanned barcode events.
  @visibleForTesting
  final eventChannel = const EventChannel(
    'dev.steenbakker.mobile_scanner/scanner/event',
  );


  StreamController<DeviceOrientation>? _deviceOrientationStreamController;
  StreamController<Map<Object?, Object?>>? _eventsStreamController;
  StreamSubscription<Object?>? _deviceOrientationSubscription;
  StreamSubscription<Object?>? _eventsSubscription;

  /// Get the event stream of device orientation change events
  /// that come from the [deviceOrientationEventChannel].
  Stream<DeviceOrientation> get deviceOrientationChangedStream {
    if (_deviceOrientationStreamController == null) {
      _deviceOrientationStreamController =
          StreamController<DeviceOrientation>.broadcast();
      _deviceOrientationSubscription = deviceOrientationEventChannel
          .receiveBroadcastStream()
          .cast<String>()
          .map((orientation) => orientation.parseDeviceOrientation())
          .listen(
            (orientation) {
              if (!(_deviceOrientationStreamController?.isClosed ?? true)) {
                _deviceOrientationStreamController!.add(orientation);
              }
            },
            onError: (Object error) {
              if (!(_deviceOrientationStreamController?.isClosed ?? true)) {
                _deviceOrientationStreamController!.addError(error);
              }
            },
            cancelOnError: false,
          );
    }

    return _deviceOrientationStreamController!.stream;
  }

  /// Get the event stream of barcode events that come from the [eventChannel].
  Stream<Map<Object?, Object?>> get eventsStream {
    if (_eventsStreamController == null) {
      _eventsStreamController =
          StreamController<Map<Object?, Object?>>.broadcast();
      _eventsSubscription = eventChannel
          .receiveBroadcastStream()
          .cast<Map<Object?, Object?>>()
          .listen(
            (event) {
              if (!(_eventsStreamController?.isClosed ?? true)) {
                _eventsStreamController!.add(event);
              }
            },
            onError: (Object error) {
              if (!(_eventsStreamController?.isClosed ?? true)) {
                _eventsStreamController!.addError(error);
              }
            },
            cancelOnError: false,
          );
    }

    return _eventsStreamController!.stream;
  }

  /// The delegate that handles texture rotation corrections on Android.
  AndroidSurfaceProducerDelegate? _surfaceProducerDelegate;

  /// Buffer of active camera instances
  final Map<int, (Future<void> Function() start, Future<void> Function() stop)>
      _instances = {};

  /// The identifier of the current texture.
  int? _textureId;
  bool _pausing = false;

  /// Parse a [BarcodeCapture] from the given [event].
  BarcodeCapture? _parseBarcode(Map<Object?, Object?>? event) {
    if (event == null) {
      return null;
    }

    final data = event['data'];

    if (data == null || data is! List<Object?>) {
      return null;
    }

    final barcodes = data.cast<Map<Object?, Object?>>();

    if (defaultTargetPlatform == TargetPlatform.android ||
        defaultTargetPlatform == TargetPlatform.iOS ||
        defaultTargetPlatform == TargetPlatform.macOS) {
      final imageData = event['image'] as Map<Object?, Object?>?;
      final image = imageData?['bytes'] as Uint8List?;
      final width = imageData?['width'] as double?;
      final height = imageData?['height'] as double?;

      return BarcodeCapture(
        raw: event,
        barcodes: barcodes.map(Barcode.fromNative).toList(),
        image: image,
        size: width == null || height == null ? Size.zero : Size(width, height),
      );
    }

    throw MobileScannerException(
      errorCode: MobileScannerErrorCode.unsupported,
      errorDetails: MobileScannerErrorDetails(
        message: MobileScannerErrorCode.unsupported.message,
      ),
    );
  }

  /// Parse a [MobileScannerBarcodeException] from the given [error] and
  /// [stackTrace], and throw it.
  ///
  /// If the error is not a [PlatformException],
  /// with [kBarcodeErrorEventName] as [PlatformException.code], the error is
  /// rethrown as-is.
  Never _parseBarcodeError(Object error, StackTrace stackTrace) {
    if (error case PlatformException(
      :final String code,
      :final String? message,
    ) when code == kBarcodeErrorEventName) {
      throw MobileScannerBarcodeException(message);
    }

    Error.throwWithStackTrace(error, stackTrace);
  }

  /// Request permission to access the camera.
  ///
  /// Throws a [MobileScannerException] if the permission is not granted.
  Future<void> _requestCameraPermission() async {
    try {
      final authorizationState = MobileScannerAuthorizationState.fromRawValue(
        await methodChannel.invokeMethod<int>(kAuthorizationStateMethodName) ??
            0,
      );

      switch (authorizationState) {
        // Authorization was already granted, no need to request it again.
        case MobileScannerAuthorizationState.authorized:
          return;
        // Android does not have an undetermined authorization state.
        // So if the permission was denied, request it again.
        case MobileScannerAuthorizationState.denied:
        case MobileScannerAuthorizationState.undetermined:
          final permissionGranted =
              await methodChannel.invokeMethod<bool>(
                kRequestAuthorizationMethodName,
              ) ??
              false;

          if (!permissionGranted) {
            throw const MobileScannerException(
              errorCode: MobileScannerErrorCode.permissionDenied,
            );
          }
      }
    } on PlatformException catch (error) {
      // If the permission state is invalid, that is an error.
      throw MobileScannerException(
        errorCode: MobileScannerErrorCode.genericError,
        errorDetails: MobileScannerErrorDetails(
          code: error.code,
          details: error.details as Object?,
          message: error.message,
        ),
      );
    }
  }

  /// Handle incoming barcode events.
  /// The error events are transformed to `MobileScannerBarcodeException` where
  /// possible.
  @override
  Stream<BarcodeCapture?> get barcodesStream {
    return eventsStream
        .where((e) => e['name'] == kBarcodeEventName)
        .map(_parseBarcode)
        .handleError(_parseBarcodeError);
  }

  @override
  Stream<TorchState> get torchStateStream {
    return eventsStream
        .where((event) => event['name'] == kTorchStateEventName)
        .map((event) => TorchState.fromRawValue(event['data'] as int? ?? 0));
  }

  @override
  Stream<double> get zoomScaleStateStream {
    return eventsStream
        .where((event) => event['name'] == kZoomScaleStateEventName)
        .map((event) => event['data'] as double? ?? 0.0);
  }

  @override
  Future<BarcodeCapture?> analyzeImage(
    String path, {
    List<BarcodeFormat> formats = const <BarcodeFormat>[],
  }) async {
    // Primary: the native ZXing-C++ engine via FFI. This is the only path on
    // desktop (Linux/Windows) and also covers the extra native formats
    // (DataBar / MaxiCode / DotCode / Code 11) on mobile. Returns empty when
    // unavailable.
    final zxingBarcodes = await analyzeImageWithZxing(path, formats);
    if (zxingBarcodes.isNotEmpty) {
      return BarcodeCapture(barcodes: zxingBarcodes);
    }

    // Fallback: the platform's native analyzer (ML Kit / Apple Vision). Absent
    // on desktop, where the method channel throws MissingPluginException.
    try {
      final result = await methodChannel.invokeMapMethod<Object?, Object?>(
        kAnalyzeImageMethodName,
        {
          'filePath': path,
          'formats':
              formats.isEmpty
                  ? null
                  : [
                    for (final BarcodeFormat format in formats)
                      if (format != BarcodeFormat.unknown) format.rawValue,
                  ],
        },
      );

      return _parseBarcode(result);
    } on MissingPluginException {
      // No native analyzer on this platform (e.g. desktop); ZXing found
      // nothing, so there is no result to report.
      return null;
    } on PlatformException catch (error) {
      // Handle any errors from analyze image requests.
      if (error.code == kBarcodeErrorEventName) {
        throw MobileScannerBarcodeException(error.message);
      }

      if (error.code == kUnsupportdOperationErrorEventName) {
        throw UnsupportedError(error.message ?? 'Unsupported operation.');
      }

      return null;
    }
  }

  @override
  Widget buildCameraView() {
    if (_textureId == null) {
      return const SizedBox();
    }

    final Widget texture = Texture(textureId: _textureId!);

    // If the preview needs manual orientation corrections,
    // correct the preview orientation based on the currently reported device
    // orientation.
    // On Android, the underlying device orientation stream will emit the
    // current orientation
    // when the first listener is attached.
    if (_surfaceProducerDelegate
        case final AndroidSurfaceProducerDelegate delegate
        when !delegate.handlesCropAndRotation) {
      return RotatedPreview.fromCameraDirection(
        delegate.cameraFacingDirection,
        deviceOrientationStream: deviceOrientationChangedStream,
        initialDeviceOrientation: delegate.initialDeviceOrientation,
        sensorOrientationDegrees: delegate.sensorOrientationDegrees,
        child: texture,
      );
    }

    return texture;
  }

  @override
  Future<void> resetZoomScale() async {
    await methodChannel.invokeMethod<void>(kResetScaleMethodName);
  }

  @override
  Future<void> setZoomScale(double zoomScale) async {
    await methodChannel.invokeMethod<void>(kSetScaleMethodName, zoomScale);
  }

  @override
  Future<void> setFocusPoint(Offset position) async {
    if (defaultTargetPlatform != TargetPlatform.iOS &&
        defaultTargetPlatform != TargetPlatform.android) {
      throw UnimplementedError('setFocusPoint() has not been implemented.');
    }

    final params = <String, Object?>{'dx': position.dx, 'dy': position.dy};

    await methodChannel.invokeMethod<void>(kSetFocusMethodName, params);
  }

  @override
  Future<MobileScannerViewAttributes> start(
    int id,
    StartOptions startOptions, {
    required Future<void> Function() startRequest,
    required Future<void> Function() stopRequest,
  }) async {
    for (final instance in _instances.entries) {
      await instance.value.$2();
    }
    _instances[id] = (
      startRequest,
      stopRequest,
    );

    if (!_pausing && _textureId != null) {
      throw MobileScannerException(
        errorCode: MobileScannerErrorCode.controllerAlreadyInitialized,
        errorDetails: MobileScannerErrorDetails(
          message: MobileScannerErrorCode.controllerAlreadyInitialized.message,
        ),
      );
    }

    await _requestCameraPermission();

    Map<String, Object?>? startResult;

    try {
      startResult = await methodChannel.invokeMapMethod<String, Object?>(
        kStartCameraMethodName,
        startOptions.toMap(),
      );
    } on PlatformException catch (error) {
      throw MobileScannerException(
        errorCode: MobileScannerErrorCode.fromPlatformException(error),
        errorDetails: MobileScannerErrorDetails(
          code: error.code,
          details: error.details as Object?,
          message: error.message,
        ),
      );
    }

    if (startResult == null) {
      throw const MobileScannerException(
        errorCode: MobileScannerErrorCode.genericError,
        errorDetails: MobileScannerErrorDetails(
          message: 'The start method did not return a view configuration.',
        ),
      );
    }

    final textureId = startResult['textureId'] as int?;

    if (textureId == null) {
      throw const MobileScannerException(
        errorCode: MobileScannerErrorCode.genericError,
        errorDetails: MobileScannerErrorDetails(
          message: 'The start method did not return a texture id.',
        ),
      );
    }

    final cameraDirection = CameraFacing.fromRawValue(
      startResult['cameraDirection'] as int?,
    );

    _textureId = textureId;

    DeviceOrientation? initialDeviceOrientation;

    if (defaultTargetPlatform == TargetPlatform.android) {
      _surfaceProducerDelegate =
          AndroidSurfaceProducerDelegate.fromConfiguration(
            startResult,
            cameraDirection,
          );
      initialDeviceOrientation =
          _surfaceProducerDelegate?.initialDeviceOrientation;
    } else if (startResult
        case {'initialDeviceOrientation': final String orientation}
        when defaultTargetPlatform == TargetPlatform.iOS ||
            defaultTargetPlatform == TargetPlatform.macOS) {
      initialDeviceOrientation = orientation.parseDeviceOrientation();
    }

    final numberOfCameras = startResult['numberOfCameras'] as int?;
    final currentTorchState = TorchState.fromRawValue(
      startResult['currentTorchState'] as int? ?? -1,
    );

    final Size size;

    if (startResult['size'] case {
      'width': final double width,
      'height': final double height,
    }) {
      size = Size(width, height);
    } else {
      size = Size.zero;
    }

    _pausing = false;

    return MobileScannerViewAttributes(
      cameraDirection: cameraDirection,
      currentTorchMode: currentTorchState,
      numberOfCameras: numberOfCameras,
      size: size,
      initialDeviceOrientation: initialDeviceOrientation,
    );
  }

  @override
  Future<void> stop({bool force = false}) async {
    if (_textureId == null && !force) {
      return;
    }

    _textureId = null;
    _pausing = false;
    _surfaceProducerDelegate = null;

    await Future.wait(
      [
        _eventsSubscription?.cancel(),
        _deviceOrientationSubscription?.cancel(),
        _eventsStreamController?.close(),
        _deviceOrientationStreamController?.close(),
      ].nonNulls,
    );

    _eventsSubscription = null;
    _deviceOrientationSubscription = null;
    _eventsStreamController = null;
    _deviceOrientationStreamController = null;

    await methodChannel.invokeMethod<void>(kStopCameraMethodName, {
      'force': force,
    });
  }

  @override
  Future<void> pause({bool force = false}) async {
    if (_pausing) {
      return;
    }

    _pausing = true;

    await methodChannel.invokeMethod<void>(kPauseCameraMethodName, {
      'force': force,
    });
  }

  @override
  Future<void> toggleTorch() async {
    await methodChannel.invokeMethod<void>(kToggleTorchMethodName);
  }

  @override
  Future<void> updateScanWindow(Rect? window) async {
    if (_textureId == null) {
      return;
    }

    List<double>? points;

    if (window != null) {
      points = [window.left, window.top, window.right, window.bottom];
    }

    await methodChannel.invokeMethod<void>(kUpdateScanWindowMethodName, {
      'rect': points,
    });
  }

  @override
  Future<Set<CameraLensType>> getSupportedLenses() async {
    final lensTypes = await methodChannel.invokeListMethod<Object?>(
      kGetSupportedLensesMethodName,
    );

    if (lensTypes == null || lensTypes.isEmpty) {
      return <CameraLensType>{};
    }

    return lensTypes.whereType<int>().map(CameraLensType.fromRawValue).toSet();
  }

  @override
  Future<void> dispose(int id) async {
    _instances.remove(id);

    await updateScanWindow(null);
    await stop();

    /// On native side, only one camera instance can be active, if we want
    /// to dispose the current camera and an other one is already up, we should
    /// not try to impact the native camera once again.
    ///
    /// If there is another camera instance available, let's trigger it
    /// (stop & start) to resume the camera instance.
    if (_instances.isNotEmpty) {
      final last = _instances.entries.last;
      await last.value.$1();
      return;
    }
  }
}
