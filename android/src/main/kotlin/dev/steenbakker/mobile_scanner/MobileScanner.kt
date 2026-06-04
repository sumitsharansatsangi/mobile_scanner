package dev.steenbakker.mobile_scanner

import android.annotation.SuppressLint
import android.app.Activity
import android.graphics.Bitmap
import android.graphics.Rect
import android.media.Image
import android.net.Uri
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.util.Size
import android.view.Surface
import androidx.annotation.VisibleForTesting
import androidx.camera.camera2.Camera2Config
import androidx.camera.core.Camera
import androidx.camera.core.CameraSelector
import androidx.camera.core.CameraXConfig
import androidx.camera.core.ExperimentalGetImage
import androidx.camera.core.ExperimentalLensFacing
import androidx.camera.core.FocusMeteringAction
import androidx.camera.core.ImageAnalysis
import androidx.camera.core.ImageAnalysis.OUTPUT_IMAGE_FORMAT_YUV_420_888
import androidx.camera.core.ImageProxy
import androidx.camera.core.MeteringPoint
import androidx.camera.core.MeteringPointFactory
import androidx.camera.core.Preview
import androidx.camera.core.SurfaceOrientedMeteringPointFactory
import androidx.camera.core.SurfaceRequest
import androidx.camera.core.TorchState
import androidx.camera.core.resolutionselector.ResolutionSelector
import androidx.camera.core.resolutionselector.ResolutionStrategy
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.core.content.ContextCompat
import androidx.lifecycle.LifecycleOwner
import com.google.mlkit.vision.barcode.BarcodeScanner
import com.google.mlkit.vision.barcode.BarcodeScannerOptions
import com.google.mlkit.vision.barcode.BarcodeScanning
import com.google.mlkit.vision.barcode.common.Barcode
import com.google.mlkit.vision.common.InputImage
import dev.steenbakker.mobile_scanner.engine.ZxingEngine
import dev.steenbakker.mobile_scanner.engine.data
import dev.steenbakker.mobile_scanner.objects.DetectionSpeed
import dev.steenbakker.mobile_scanner.objects.MobileScannerErrorCodes
import dev.steenbakker.mobile_scanner.objects.MobileScannerStartParameters
import dev.steenbakker.mobile_scanner.utils.invertBitmapColors
import dev.steenbakker.mobile_scanner.utils.rotateBitmap
import dev.steenbakker.mobile_scanner.utils.serialize
import io.flutter.view.TextureRegistry
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import java.io.ByteArrayOutputStream
import java.io.IOException
import java.util.concurrent.Executors
import kotlin.math.roundToInt

class MobileScanner(
    private val activity: Activity,
    private val textureRegistry: TextureRegistry,
    private val mobileScannerCallback: MobileScannerCallback,
    private val mobileScannerErrorCallback: MobileScannerErrorCallback,
    private val deviceOrientationListener: DeviceOrientationListener,
    private val barcodeScannerFactory: (options: BarcodeScannerOptions?) -> BarcodeScanner = ::defaultBarcodeScannerFactory,
) {

    init {
        configureCameraProcessProvider()
    }

    /// Internal variables
    private var cameraProvider: ProcessCameraProvider? = null
    private var camera: Camera? = null
    private var cameraSelector: CameraSelector? = null
    private var preview: Preview? = null
    private var surfaceProducer: TextureRegistry.SurfaceProducer? = null
    private var scanner: BarcodeScanner? = null
    private var lastScanned: List<String?>? = null
    private var scannerTimeout = false
    private var imageAnalysis: ImageAnalysis? = null
    private var analysisExecutor = Executors.newSingleThreadExecutor()

    /// Configurable variables
    var scanWindow: List<Float>? = null
    var shouldConsiderInvertedImages: Boolean = false
    private var invertImage: Boolean = false
    private var detectionSpeed: DetectionSpeed = DetectionSpeed.NO_DUPLICATES
    private var detectionTimeout: Long = 250
    private var returnImage = false
    private var isPaused = false

    // Advanced features for breaking change release
    private var enableAdvancedProcessing: Boolean = true
    private var enableQualityAnalysis: Boolean = false
    private var enableBatchProcessing: Boolean = false
    private var enhanceImageQuality: Boolean = true

    // Hybrid engine: ZXing-C++ is the primary decoder; ML Kit is the recovery
    // fallback. The native engine is loaded lazily and disabled if unavailable.
    private val zxingEngine = ZxingEngine()
    private val zxingAvailable: Boolean by lazy { ZxingEngine.ensureLoaded() }

    /// OR-ed BarcodeFormat.rawValue bits to scan for (0 = all formats).
    private var zxingFormatMask: Int = 0

    /// Number of consecutive frames ZXing must find nothing in before the
    /// ML Kit recovery path is invoked. Keeps ML Kit off the hot path for
    /// ordinary frames while still recovering hard-to-read barcodes.
    private var zxingFallbackThreshold: Int = 3
    private var consecutiveZxingMisses: Int = 0

    companion object {
        // Configure the `ProcessCameraProvider` to only log errors.
        // This prevents the informational log spam from CameraX.
        private fun configureCameraProcessProvider() {
            try {
                val config = CameraXConfig.Builder.fromConfig(Camera2Config.defaultConfig()).apply {
                    setMinimumLoggingLevel(Log.ERROR)
                }
                ProcessCameraProvider.configureInstance(config.build())
            } catch (_: IllegalStateException) {
                // The ProcessCameraProvider was already configured.
                // Do nothing.
            }
        }

        /**
         * Create a barcode scanner from the given options.
         */
        fun defaultBarcodeScannerFactory(options: BarcodeScannerOptions?) : BarcodeScanner {
            return if (options == null) BarcodeScanning.getClient() else BarcodeScanning.getClient(options)
        }
    }

    /**
     * callback for the camera. Every frame is passed through this function.
     */
    @ExperimentalGetImage
    val captureOutput = ImageAnalysis.Analyzer { imageProxy ->
        val mediaImage = imageProxy.image ?: return@Analyzer
        if (detectionSpeed == DetectionSpeed.NORMAL && scannerTimeout) {
            imageProxy.close()
            return@Analyzer
        } else if (detectionSpeed == DetectionSpeed.NORMAL) {
            scannerTimeout = true
        }

        // Invert every other frame if shouldConsiderInvertedImages is enabled
        if (shouldConsiderInvertedImages) {
            invertImage = !invertImage // so we jump from one normal to one inverted and viceversa
        }

        // Create InputImage directly from ImageProxy for better performance
        // Only convert to Bitmap if we need to invert colors
        var invertedBitmap: Bitmap? = null
        val inputImage = if (invertImage) {
            val bitmap = imageProxy.toBitmap()
            invertBitmapColors(bitmap)
            invertedBitmap = bitmap
            InputImage.fromBitmap(bitmap, imageProxy.imageInfo.rotationDegrees)
        } else {
            InputImage.fromMediaImage(mediaImage, imageProxy.imageInfo.rotationDegrees)
        }

        if (detectionSpeed == DetectionSpeed.NORMAL && scannerTimeout) {
            imageProxy.close()
            return@Analyzer
        } else if (detectionSpeed == DetectionSpeed.NORMAL) {
            scannerTimeout = true
        }

        // === ZXing-C++ primary fast path ===
        // ML Kit is only consulted after a run of frames ZXing can't read.
        if (zxingAvailable) {
            val portrait = (camera?.cameraInfo?.sensorRotationDegrees ?: 0) % 180 == 0
            val reportWidth = if (portrait) inputImage.width else inputImage.height
            val reportHeight = if (portrait) inputImage.height else inputImage.width

            val zxingBarcodes = tryZxingDecode(mediaImage)
            val barcodeMap = zxingBarcodes
                .filter {
                    scanWindow == null || isZxingBarcodeInScanWindow(
                        scanWindow!!, it.corners, mediaImage.width, mediaImage.height,
                    )
                }
                .map { it.data }

            if (barcodeMap.isNotEmpty()) {
                consecutiveZxingMisses = 0

                if (detectionSpeed == DetectionSpeed.NO_DUPLICATES) {
                    val newScanned = barcodeMap
                        .mapNotNull { it["rawValue"] as String? }
                        .sorted()
                    if (newScanned == lastScanned) {
                        invertedBitmap?.recycle()
                        imageProxy.close()
                        scheduleScannerTimeoutReset()
                        return@Analyzer
                    }
                    if (newScanned.isNotEmpty()) {
                        lastScanned = newScanned
                    }
                }

                emitBarcodes(barcodeMap, imageProxy, invertedBitmap, reportWidth, reportHeight)
                scheduleScannerTimeoutReset()
                return@Analyzer
            }

            // ZXing read nothing this frame.
            consecutiveZxingMisses++
            if (consecutiveZxingMisses < zxingFallbackThreshold) {
                invertedBitmap?.recycle()
                imageProxy.close()
                scheduleScannerTimeoutReset()
                return@Analyzer
            }
            // Threshold reached: fall through to the ML Kit recovery path once.
            consecutiveZxingMisses = 0
        }

        scanner?.let {
            it.process(inputImage).addOnSuccessListener { barcodes ->
                if (detectionSpeed == DetectionSpeed.NO_DUPLICATES) {
                    val newScannedBarcodes = barcodes.mapNotNull {
                        barcode -> barcode.rawValue
                    }.sorted()

                    if (newScannedBarcodes == lastScanned) {
                        // New scanned is duplicate, returning
                        imageProxy.close()
                        return@addOnSuccessListener
                    }
                    if (newScannedBarcodes.isNotEmpty()) {
                        lastScanned = newScannedBarcodes
                    }
                }

                val barcodeMap: MutableList<Map<String, Any?>> = mutableListOf()

                for (barcode in barcodes) {
                    if (scanWindow == null) {
                        barcodeMap.add(barcode.data)
                        continue
                    }

                    if (isBarcodeInScanWindow(scanWindow!!, barcode, imageProxy)) {
                        barcodeMap.add(barcode.data)
                    }
                }

                if (barcodeMap.isEmpty()) {
                    imageProxy.close()
                    return@addOnSuccessListener
                }

                val portrait = (camera?.cameraInfo?.sensorRotationDegrees ?: 0) % 180 == 0

                emitBarcodes(
                    barcodeMap,
                    imageProxy,
                    invertedBitmap,
                    if (portrait) inputImage.width else inputImage.height,
                    if (portrait) inputImage.height else inputImage.width,
                )
            }.addOnFailureListener { e ->
                mobileScannerErrorCallback(
                    e.localizedMessage ?: e.toString()
                )
            }
        }

        scheduleScannerTimeoutReset()
    }

    /**
     * In [DetectionSpeed.NORMAL], re-arm the throttle timer so the next frame is
     * processed after [detectionTimeout]. No-op in other detection speeds.
     */
    private fun scheduleScannerTimeoutReset() {
        if (detectionSpeed == DetectionSpeed.NORMAL) {
            Handler(Looper.getMainLooper()).postDelayed({
                scannerTimeout = false
            }, detectionTimeout)
        }
    }

    /**
     * Decode the luminance (Y) plane of [mediaImage] with the native ZXing-C++
     * engine. Returns an empty list on any failure so the caller can fall back
     * to ML Kit.
     */
    @ExperimentalGetImage
    private fun tryZxingDecode(mediaImage: Image): List<dev.steenbakker.mobile_scanner.engine.ZxingBarcode> {
        return try {
            val yPlane = mediaImage.planes[0]
            zxingEngine.decodeLuma(
                luma = yPlane.buffer,
                width = mediaImage.width,
                height = mediaImage.height,
                rowStride = yPlane.rowStride,
                formatMask = zxingFormatMask,
                tryHarder = false,
                tryRotate = true,
                tryInvert = shouldConsiderInvertedImages || invertImage,
            )
        } catch (_: Throwable) {
            emptyList()
        }
    }

    /**
     * Scan-window containment test for ZXing results.
     *
     * NOTE: ZXing corners are in sensor (unrotated) space while [scanWindow] is
     * normalized in preview space; for non-trivial rotations this mapping may
     * need refinement and should be validated on-device.
     */
    private fun isZxingBarcodeInScanWindow(
        scanWindow: List<Float>,
        corners: FloatArray,
        imageWidth: Int,
        imageHeight: Int,
    ): Boolean {
        return try {
            val rect = Rect(
                (scanWindow[0] * imageWidth).roundToInt(),
                (scanWindow[1] * imageHeight).roundToInt(),
                (scanWindow[2] * imageWidth).roundToInt(),
                (scanWindow[3] * imageHeight).roundToInt(),
            )
            var i = 0
            while (i < corners.size - 1) {
                if (!rect.contains(corners[i].roundToInt(), corners[i + 1].roundToInt())) {
                    return false
                }
                i += 2
            }
            true
        } catch (_: IllegalArgumentException) {
            false
        }
    }

    /**
     * Emit [barcodeMap] to Flutter, optionally attaching a JPEG of the frame
     * when [returnImage] is set. Shared by the ZXing and ML Kit paths. Always
     * closes [imageProxy].
     */
    private fun emitBarcodes(
        barcodeMap: List<Map<String, Any?>>,
        imageProxy: ImageProxy,
        invertedBitmap: Bitmap?,
        reportWidth: Int,
        reportHeight: Int,
    ) {
        if (!returnImage) {
            mobileScannerCallback(barcodeMap, null, reportWidth, reportHeight)
            invertedBitmap?.recycle()
            imageProxy.close()
            return
        }

        // Generate the JPEG off the main thread to keep the preview smooth.
        CoroutineScope(Dispatchers.IO).launch {
            val baseBitmap = invertedBitmap ?: imageProxy.toBitmap()
            val rotatedBitmap =
                rotateBitmap(baseBitmap, camera?.cameraInfo?.sensorRotationDegrees ?: 90)

            if (invertImage) {
                invertBitmapColors(rotatedBitmap)
            }
            if (baseBitmap != rotatedBitmap) {
                baseBitmap.recycle()
            }

            val stream = ByteArrayOutputStream()
            rotatedBitmap.compress(Bitmap.CompressFormat.JPEG, 80, stream)

            mobileScannerCallback(
                barcodeMap,
                stream.toByteArray(),
                rotatedBitmap.width,
                rotatedBitmap.height,
            )

            rotatedBitmap.recycle()
            imageProxy.close()
        }
    }

    /**
     * Create a {@link Preview.SurfaceProvider} that specifies how to provide a {@link Surface} to a
     * {@code Preview}.
     */
    @VisibleForTesting
    fun createSurfaceProvider(surfaceProducer: TextureRegistry.SurfaceProducer): Preview.SurfaceProvider {
        return Preview.SurfaceProvider {
            request: SurfaceRequest ->
            run {
                // Set the callback for the surfaceProducer to invalidate Surfaces that it produces
                // when they get destroyed.
                surfaceProducer.setCallback(
                    object : TextureRegistry.SurfaceProducer.Callback {
                        override fun onSurfaceAvailable() {
                            // Do nothing. The Preview.SurfaceProvider will handle this
                            // whenever a new Surface is needed.
                        }

                        override fun onSurfaceCleanup() {
                            // Invalidate the SurfaceRequest so that CameraX knows to to make a new request
                            // for a surface.
                            request.invalidate()
                        }
                    }
                )

                // Provide the surface.
                surfaceProducer.setSize(request.resolution.width, request.resolution.height)

                val surface: Surface = surfaceProducer.surface

                // The single thread executor is only used to invoke the result callback.
                // Thus it is safe to use a new executor,
                // instead of reusing the executor that is passed to the camera process provider.
                request.provideSurface(surface, Executors.newSingleThreadExecutor()) {
                    // Handle the result of the request for a surface.
                    // See: https://developer.android.com/reference/androidx/camera/core/SurfaceRequest.Result

                    // Always attempt a release.
                    surface.release()

                    val resultCode: Int = it.resultCode

                    when(resultCode) {
                        SurfaceRequest.Result.RESULT_REQUEST_CANCELLED,
                        SurfaceRequest.Result.RESULT_WILL_NOT_PROVIDE_SURFACE,
                        SurfaceRequest.Result.RESULT_SURFACE_ALREADY_PROVIDED,
                        SurfaceRequest.Result.RESULT_SURFACE_USED_SUCCESSFULLY -> {
                            // Only need to release, do nothing.
                        }
                        SurfaceRequest.Result.RESULT_INVALID_SURFACE -> {
                            // The surface was invalid, so it is not clear how to recover from this.
                        }
                        else -> {
                            // Fallthrough, in case any result codes are added later.
                        }
                    }
                }
            }
        }
    }

    @ExperimentalLensFacing
    private fun getCameraLensFacing(camera: Camera?): Int? {
        return when(camera?.cameraInfo?.lensFacing) {
            CameraSelector.LENS_FACING_BACK -> 1
            CameraSelector.LENS_FACING_FRONT -> 0
            CameraSelector.LENS_FACING_EXTERNAL -> 2
            CameraSelector.LENS_FACING_UNKNOWN -> null
            else -> null
        }
    }

    // Scales the scanWindow to the provided inputImage and checks if that scaled
    // scanWindow contains the barcode.
    @VisibleForTesting
    fun isBarcodeInScanWindow(
        scanWindow: List<Float>,
        barcode: Barcode,
        inputImage: ImageProxy
    ): Boolean {
        val cornerPoints = barcode.cornerPoints ?: return false

        try {
            val rotationDegrees = inputImage.imageInfo.rotationDegrees
            val imageWidth = if (rotationDegrees % 180 == 0) inputImage.width else inputImage.height
            val imageHeight = if (rotationDegrees % 180 == 0) inputImage.height else inputImage.width

            val left = (scanWindow[0] * imageWidth).roundToInt()
            val top = (scanWindow[1] * imageHeight).roundToInt()
            val right = (scanWindow[2] * imageWidth).roundToInt()
            val bottom = (scanWindow[3] * imageHeight).roundToInt()

            val scaledScanWindow = Rect(left, top, right, bottom)

            return cornerPoints.all { scaledScanWindow.contains(it.x, it.y) }
        } catch (_: IllegalArgumentException) {
            // Rounding of the scan window dimensions can fail, due to encountering NaN.
            // If we get NaN, rather than give a false positive, just return false.
            return false
        }
    }

    /**
     * Start barcode scanning by initializing the camera and barcode scanner.
     */
    @ExperimentalLensFacing
    @ExperimentalGetImage
    fun start(
        barcodeScannerOptions: BarcodeScannerOptions?,
        returnImage: Boolean,
        cameraPosition: CameraSelector,
        torch: Boolean,
        detectionSpeed: DetectionSpeed,
        torchStateCallback: TorchStateCallback,
        zoomScaleStateCallback: ZoomScaleStateCallback,
        mobileScannerStartedCallback: MobileScannerStartedCallback,
        mobileScannerErrorCallback: (exception: Exception) -> Unit,
        detectionTimeout: Long,
        cameraResolution: Size?,
        useNewCameraSelector: Boolean,
        invertImage: Boolean,
        shouldConsiderInvertedImages: Boolean,
        initialZoom: Double?,
        enableAdvancedProcessing: Boolean = true,
        enableQualityAnalysis: Boolean = false,
        enableBatchProcessing: Boolean = false,
        enhanceImageQuality: Boolean = true,
        formats: List<Int>? = null,
    ) {
        this.detectionSpeed = detectionSpeed
        this.detectionTimeout = detectionTimeout
        this.returnImage = returnImage
        this.shouldConsiderInvertedImages = shouldConsiderInvertedImages
        this.invertImage = invertImage
        this.enableAdvancedProcessing = enableAdvancedProcessing
        this.enableQualityAnalysis = enableQualityAnalysis
        this.enableBatchProcessing = enableBatchProcessing
        this.enhanceImageQuality = enhanceImageQuality

        // Build the ZXing format mask from the requested rawValue bits. An empty
        // or null list means "all formats" (mask 0).
        // itf2of5 (126) and itf2of5WithChecksum (127) are not power-of-two
        // flags; their values collide with combinations of the other format
        // bits, so normalize them onto the dedicated ITF bit (itf14 == 128)
        // before OR-ing. zxing-cpp has a single ITF format.
        zxingFormatMask = formats?.fold(0) { acc, raw ->
            acc or if (raw == 126 || raw == 127) 128 else raw
        } ?: 0
        consecutiveZxingMisses = 0

        isPaused = false

        if (camera?.cameraInfo != null && preview != null && surfaceProducer != null && !isPaused) {

// TODO: resume here for seamless transition
//            if (isPaused) {
//                resumeCamera()
//                val cameraDirection = getCameraLensFacing(camera)
//                mobileScannerStartedCallback(
//                  MobileScannerStartParameters(
//                    if (portrait) width else height,
//                    if (portrait) height else width,
//                    deviceOrientationListener.getOrientation().serialize(),
//                    sensorRotationDegrees,
//                    surfaceProducer!!.handlesCropAndRotation(),
//                    currentTorchState,
//                    surfaceProducer!!.id(),
//                    numberOfCameras ?: 0,
//                    cameraDirection
//                  )
//                )
//                return
//            }
            mobileScannerErrorCallback(AlreadyStarted())

            return
        }

        lastScanned = null
        scanner = barcodeScannerFactory(barcodeScannerOptions)

        val cameraProviderFuture = ProcessCameraProvider.getInstance(activity)
        val mainExecutor = ContextCompat.getMainExecutor(activity)

        cameraProviderFuture.addListener({
            cameraProvider = cameraProviderFuture.get()
            val numberOfCameras = cameraProvider?.availableCameraInfos?.size

            if (cameraProvider == null) {
                mobileScannerErrorCallback(CameraError())

                return@addListener
            }

            cameraProvider?.unbindAll()
            surfaceProducer = surfaceProducer ?: textureRegistry.createSurfaceProducer()
            val surfaceProvider: Preview.SurfaceProvider = createSurfaceProvider(surfaceProducer!!)

            // Preview

            // Build the preview to be shown on the Flutter texture
            val previewBuilder = Preview.Builder()
            preview = previewBuilder.build().apply { setSurfaceProvider(surfaceProvider) }

            // Build the analyzer to be passed on to MLKit
            val analysisBuilder = ImageAnalysis.Builder()
                .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
                .setOutputImageFormat(OUTPUT_IMAGE_FORMAT_YUV_420_888)

            val selectedResolution = cameraResolution ?: Size(1920, 1080)

            val selectorBuilder = ResolutionSelector.Builder()
            selectorBuilder.setResolutionStrategy(
                ResolutionStrategy(
                    selectedResolution,
                    ResolutionStrategy.FALLBACK_RULE_CLOSEST_HIGHER_THEN_LOWER
                )
            )
            analysisBuilder.setResolutionSelector(selectorBuilder.build()).build()

            deviceOrientationListener.onDisplayRotationChanged = { rotation ->
                imageAnalysis?.targetRotation = rotation
            }

            val analysis = analysisBuilder.build().apply { setAnalyzer(analysisExecutor, captureOutput) }
            imageAnalysis = analysis

            try {
                camera = cameraProvider?.bindToLifecycle(
                    activity as LifecycleOwner,
                    cameraPosition,
                    preview,
                    analysis
                )
                cameraSelector = cameraPosition
            } catch(_: Exception) {
                mobileScannerErrorCallback(NoCamera())

                return@addListener
            }

            camera?.let {
                // Register the torch listener
                it.cameraInfo.torchState.observe(activity as LifecycleOwner) { state ->
                    // TorchState.OFF = 0; TorchState.ON = 1
                    torchStateCallback(state)
                }

                // Register the zoom scale listener
                it.cameraInfo.zoomState.observe(activity) { state ->
                    zoomScaleStateCallback(state.linearZoom.toDouble())
                }

                // Enable torch if provided
                if (it.cameraInfo.hasFlashUnit()) {
                    it.cameraControl.enableTorch(torch)
                }

                if (initialZoom != null) {
                    try {
                        if (initialZoom in 0.0..1.0) {
                            it.cameraControl.setLinearZoom(initialZoom.toFloat())
                        } else {
                            it.cameraControl.setZoomRatio(initialZoom.toFloat())
                        }
                    } catch (e: Exception) {
                        mobileScannerErrorCallback(ZoomNotInRange())

                        return@addListener
                    }
                }
            }

            val resolution = analysis.resolutionInfo!!.resolution
            val width = resolution.width.toDouble()
            val height = resolution.height.toDouble()
            val sensorRotationDegrees = camera?.cameraInfo?.sensorRotationDegrees ?: 0
            val portrait = sensorRotationDegrees % 180 == 0
            val cameraDirection = getCameraLensFacing(camera)

            // Start with 'unavailable' torch state.
            var currentTorchState: Int = -1

            camera?.cameraInfo?.let {
                if (!it.hasFlashUnit()) {
                    return@let
                }

                currentTorchState = it.torchState.value ?: -1
            }

            deviceOrientationListener.start()

            mobileScannerStartedCallback(
                MobileScannerStartParameters(
                    if (portrait) width else height,
                    if (portrait) height else width,
                    deviceOrientationListener.getOrientation().serialize(),
                    sensorRotationDegrees,
                    surfaceProducer!!.handlesCropAndRotation(),
                    currentTorchState,
                    surfaceProducer!!.id(),
                    numberOfCameras ?: 0,
                    cameraDirection,
                )
            )
        }, mainExecutor)

    }

    /**
     * Pause barcode scanning.
     */
    fun pause(force: Boolean = false) {
        if (!force) {
            if (isPaused) {
                throw AlreadyPaused()
            } else if (isStopped()) {
                throw AlreadyStopped()
            }
        }

        deviceOrientationListener.stop()
        pauseCamera()
    }

    /**
     * Stop barcode scanning.
     */
    fun stop(force: Boolean = false) {
        if (!force) {
            if (!isPaused && isStopped()) {
                throw AlreadyStopped()
            }
        }

        deviceOrientationListener.stop()
        releaseCamera()
    }

    private fun pauseCamera() {
        // Pause camera by unbinding all use cases
        cameraProvider?.unbindAll()
        isPaused = true
    }

//    private fun resumeCamera() {
//        // Resume camera by rebinding use cases
//        cameraProvider?.let { provider ->
//            val owner = activity as LifecycleOwner
//            cameraSelector?.let { provider.bindToLifecycle(owner, it, preview) }
//        }
//        isPaused = false
//    }

    private fun releaseCamera() {
        val owner = activity as LifecycleOwner
        // Release the camera observers first.
        camera?.cameraInfo?.let {
            it.torchState.removeObservers(owner)
            it.zoomState.removeObservers(owner)
            it.cameraState.removeObservers(owner)
        }

        // Unbind the camera use cases, the preview is a use case.
        // The camera will be closed when the last use case is unbound.
        cameraProvider?.unbindAll()
        camera = null
        preview = null
        imageAnalysis = null

        // Release the surface for the preview.
        surfaceProducer?.release()
        surfaceProducer = null

        // Release the scanner.
        scanner?.close()
        scanner = null
        lastScanned = null

        // Shutdown the analysis executor
        analysisExecutor.shutdown()
        // Create a new executor for potential restart
        analysisExecutor = Executors.newSingleThreadExecutor()
    }

    private fun isStopped() = camera == null && preview == null

    /**
     * Toggles the flash light on or off.
     */
    fun toggleTorch() {
        camera?.let {
            if (!it.cameraInfo.hasFlashUnit()) {
                return@let
            }

            when(it.cameraInfo.torchState.value) {
                TorchState.OFF -> it.cameraControl.enableTorch(true)
                TorchState.ON -> it.cameraControl.enableTorch(false)
            }
        }
    }

    /**
     * Inverts the image colours respecting the alpha channel
     */
    fun invertInputImage(imageProxy: ImageProxy): InputImage {
        val bitmap = imageProxy.toBitmap()
        invertBitmapColors(bitmap)
        return InputImage.fromBitmap(bitmap, imageProxy.imageInfo.rotationDegrees)
    }

    // Helper function to invert the colors of the bitmap
    private fun invertBitmapColors(bitmap: Bitmap) {
        val width = bitmap.width
        val height = bitmap.height
        for (x in 0 until width) {
            for (y in 0 until height) {
                val pixel = bitmap.getPixel(x, y)
                val invertedColor = invertColor(pixel)
                bitmap.setPixel(x, y, invertedColor)
            }
        }
    }

    private fun invertColor(pixel: Int): Int {
        val alpha = pixel and 0xFF000000.toInt()
        val red = 255 - (pixel shr 16 and 0xFF)
        val green = 255 - (pixel shr 8 and 0xFF)
        val blue = 255 - (pixel and 0xFF)
        return alpha or (red shl 16) or (green shl 8) or blue
    }

    /**
     * Analyze a single image.
     */
    fun analyzeImage(
        image: Uri,
        scannerOptions: BarcodeScannerOptions?,
        onSuccess: AnalyzerSuccessCallback,
        onError: AnalyzerErrorCallback) {
        val inputImage: InputImage

        try {
            inputImage = InputImage.fromFilePath(activity, image)
        } catch (_: IOException) {
            onError(MobileScannerErrorCodes.ANALYZE_IMAGE_NO_VALID_IMAGE_ERROR_MESSAGE)

            return
        }

        // Use a short lived scanner instance, which is closed when the analysis is done.
        val barcodeScanner: BarcodeScanner = barcodeScannerFactory(scannerOptions)

        barcodeScanner.process(inputImage).addOnSuccessListener { barcodes ->
            val barcodeMap = barcodes.map { barcode -> barcode.data }

            onSuccess(barcodeMap)
        }.addOnFailureListener { e ->
            onError(e.localizedMessage ?: e.toString())
        }.addOnCompleteListener {
            barcodeScanner.close()
        }
    }

    fun setZoomRatio(zoomRatio: Double) {
        if (camera == null) throw ZoomWhenStopped()
        camera?.cameraControl?.setZoomRatio(zoomRatio.toFloat())
    }

    /**
     * Set the zoom rate of the camera.
     */
    fun setScale(scale: Double) {
        if (scale > 1.0 || scale < 0) throw ZoomNotInRange()
        if (camera == null) throw ZoomWhenStopped()
        camera?.cameraControl?.setLinearZoom(scale.toFloat())
    }

    /**
     * Reset the zoom rate of the camera.
     */
    fun resetScale() {
        if (camera == null) throw ZoomWhenStopped()
        camera?.cameraControl?.setZoomRatio(1f)
    }

    fun setFocus(x: Float, y: Float) {
        val cam = camera ?: throw ZoomWhenStopped()

        // Ensure x,y are normalized (0f..1f)
        if (x !in 0f..1f || y !in 0f..1f) {
            throw IllegalArgumentException("Focus coordinates must be between 0.0 and 1.0")
        }

        val factory: MeteringPointFactory = SurfaceOrientedMeteringPointFactory(1f, 1f)
        val afPoint: MeteringPoint = factory.createPoint(x, y)

        val action = FocusMeteringAction.Builder(afPoint, FocusMeteringAction.FLAG_AF)
            .build()

        cam.cameraControl.startFocusAndMetering(action)
    }

    /**
     * Dispose of this scanner instance.
     */
    fun dispose() {
        if (isStopped()) {
            return
        }

        stop() // Defer to the stop method, which disposes all resources anyway.
    }
}
