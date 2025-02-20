package dev.steenbakker.mobile_scanner

import android.annotation.SuppressLint
import android.app.Activity
import android.content.Context
import android.graphics.Bitmap
import android.graphics.Matrix
import android.graphics.Rect
import android.hardware.display.DisplayManager
import android.media.Image
import android.net.Uri
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.util.Size
import android.view.Surface
import android.view.WindowManager
import androidx.annotation.VisibleForTesting
import androidx.camera.core.Camera
import androidx.camera.core.CameraSelector
import androidx.camera.core.ExperimentalGetImage
import androidx.camera.core.ImageAnalysis
import androidx.camera.core.ImageProxy
import androidx.camera.core.Preview
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
import dev.steenbakker.mobile_scanner.objects.DetectionSpeed
import dev.steenbakker.mobile_scanner.objects.MobileScannerStartParameters
import dev.steenbakker.mobile_scanner.utils.YuvToRgbConverter
import io.flutter.view.TextureRegistry
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.launch
import java.io.ByteArrayOutputStream
import kotlin.math.roundToInt

class MobileScanner(
    private val activity: Activity,
    private val textureRegistry: TextureRegistry,
    private val mobileScannerCallback: MobileScannerCallback,
    private val mobileScannerErrorCallback: MobileScannerErrorCallback,
    private val barcodeScannerFactory: (options: BarcodeScannerOptions?) -> BarcodeScanner = ::defaultBarcodeScannerFactory,
) {

    /// Internal variables
    private var cameraProvider: ProcessCameraProvider? = null
    private var camera: Camera? = null
    private var cameraSelector: CameraSelector? = null
    private var preview: Preview? = null
    private var textureEntry: TextureRegistry.SurfaceTextureEntry? = null
    private var scanner: BarcodeScanner? = null
    private var lastScanned: List<String?>? = null
    private var scannerTimeout = false
    private var displayListener: DisplayManager.DisplayListener? = null

    /// Configurable variables
    var scanWindow: List<Float>? = null
    var shouldConsiderInvertedImages: Boolean = false
    private var invertCurrentImage: Boolean = false
    private var detectionSpeed: DetectionSpeed = DetectionSpeed.NO_DUPLICATES
    private var detectionTimeout: Long = 250
    private var returnImage = false
    private var isPaused = false

    companion object {
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
    val captureOutput = ImageAnalysis.Analyzer { imageProxy -> // YUV_420_888 format
        val mediaImage = imageProxy.image ?: return@Analyzer
         // Invert every other frame.
        if (shouldConsiderInvertedImages) {
            invertCurrentImage = !invertCurrentImage // so we jump from one normal to one inverted and viceversa
        }

        val inputImage = if (invertCurrentImage) {
            invertInputImage(imageProxy)
        } else {
            InputImage.fromMediaImage(mediaImage, imageProxy.imageInfo.rotationDegrees)
        }

        if (detectionSpeed == DetectionSpeed.NORMAL && scannerTimeout) {
            imageProxy.close()
            return@Analyzer
        } else if (detectionSpeed == DetectionSpeed.NORMAL) {
            scannerTimeout = true
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

                if (!returnImage) {
                    mobileScannerCallback(
                        barcodeMap,
                        null,
                        mediaImage.width,
                        mediaImage.height)
                    imageProxy.close()
                    return@addOnSuccessListener
                }

                CoroutineScope(Dispatchers.IO).launch {
                    val bitmap = Bitmap.createBitmap(mediaImage.width, mediaImage.height, Bitmap.Config.ARGB_8888)
                    val imageFormat = YuvToRgbConverter(activity.applicationContext)

                    imageFormat.yuvToRgb(mediaImage, bitmap)

                    val bmResult = rotateBitmap(bitmap, camera?.cameraInfo?.sensorRotationDegrees?.toFloat() ?: 90f)

                    val stream = ByteArrayOutputStream()
                    bmResult.compress(Bitmap.CompressFormat.PNG, 100, stream)
                    val byteArray = stream.toByteArray()
                    val bmWidth = bmResult.width
                    val bmHeight = bmResult.height

                    mobileScannerCallback(
                        barcodeMap,
                        byteArray,
                        bmWidth,
                        bmHeight
                    )

                    bmResult.recycle()
                    imageProxy.close()
                    imageFormat.release()
                }

            }.addOnFailureListener { e ->
                mobileScannerErrorCallback(
                    e.localizedMessage ?: e.toString()
                )
            }
        }

        if (detectionSpeed == DetectionSpeed.NORMAL) {
            // Set timer and continue
            Handler(Looper.getMainLooper()).postDelayed({
                scannerTimeout = false
            }, detectionTimeout)
        }
    }

    private fun rotateBitmap(bitmap: Bitmap, degrees: Float): Bitmap {
        val matrix = Matrix()
        matrix.postRotate(degrees)
        return Bitmap.createBitmap(bitmap, 0, 0, bitmap.width, bitmap.height, matrix, true)
    }

    // Scales the scanWindow to the provided inputImage and checks if that scaled
    // scanWindow contains the barcode.
    @VisibleForTesting
    fun isBarcodeInScanWindow(
        scanWindow: List<Float>,
        barcode: Barcode,
        inputImage: ImageProxy
    ): Boolean {
        // TODO: use `cornerPoints` instead, since the bounding box is not bound to the coordinate system of the input image
        // On iOS we do this correctly, so the calculation should match that.
        val barcodeBoundingBox = barcode.boundingBox ?: return false

        try {
            val imageWidth = inputImage.height
            val imageHeight = inputImage.width

            val left = (scanWindow[0] * imageWidth).roundToInt()
            val top = (scanWindow[1] * imageHeight).roundToInt()
            val right = (scanWindow[2] * imageWidth).roundToInt()
            val bottom = (scanWindow[3] * imageHeight).roundToInt()

            val scaledScanWindow = Rect(left, top, right, bottom)

            return scaledScanWindow.contains(barcodeBoundingBox)
        } catch (exception: IllegalArgumentException) {
            // Rounding of the scan window dimensions can fail, due to encountering NaN.
            // If we get NaN, rather than give a false positive, just return false.
            return false
        }
    }

    // Return the best resolution for the actual device orientation.
    //
    // By default the resolution is 480x640, which is too low for ML Kit.
    // If the given resolution is not supported by the display,
    // the closest available resolution is used.
    //
    // The resolution should be adjusted for the display rotation, to preserve the aspect ratio.
    @Suppress("deprecation")
    private fun getResolution(cameraResolution: Size): Size {
        val rotation = if (Build.VERSION.SDK_INT >= 30) {
            activity.display!!.rotation
        } else {
            val windowManager = activity.applicationContext.getSystemService(Context.WINDOW_SERVICE) as WindowManager

            windowManager.defaultDisplay.rotation
        }

        val widthMaxRes = cameraResolution.width
        val heightMaxRes = cameraResolution.height

        val targetResolution = if (rotation == Surface.ROTATION_0 || rotation == Surface.ROTATION_180) {
            Size(widthMaxRes, heightMaxRes) // Portrait mode
        } else {
            Size(heightMaxRes, widthMaxRes) // Landscape mode
        }
        return targetResolution
    }

    /**
     * Start barcode scanning by initializing the camera and barcode scanner.
     */
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
        newCameraResolutionSelector: Boolean,
        shouldConsiderInvertedImages: Boolean,
    ) {
        this.detectionSpeed = detectionSpeed
        this.detectionTimeout = detectionTimeout
        this.returnImage = returnImage
        this.shouldConsiderInvertedImages = shouldConsiderInvertedImages
        if (camera?.cameraInfo != null && preview != null && textureEntry != null && !isPaused) {

           // TODO: resume here for seamless transition
//            if (isPaused) {
//                resumeCamera()
//                mobileScannerStartedCallback(
//                    MobileScannerStartParameters(
//                        if (portrait) width else height,
//                        if (portrait) height else width,
//                        currentTorchState,
//                        textureEntry!!.id(),
//                        numberOfCameras ?: 0
//                    )
//                )
//                return
//            }
            mobileScannerErrorCallback(AlreadyStarted())

            return
        }

        lastScanned = null
        scanner = barcodeScannerFactory(barcodeScannerOptions)

        val cameraProviderFuture = ProcessCameraProvider.getInstance(activity)
        val executor = ContextCompat.getMainExecutor(activity)

        cameraProviderFuture.addListener({
            cameraProvider = cameraProviderFuture.get()
            val numberOfCameras = cameraProvider?.availableCameraInfos?.size

            if (cameraProvider == null) {
                mobileScannerErrorCallback(CameraError())

                return@addListener
            }

            cameraProvider?.unbindAll()
            textureEntry = textureEntry ?: textureRegistry.createSurfaceTexture()

            // Preview
            val surfaceProvider = Preview.SurfaceProvider { request ->
                if (isStopped()) {
                    return@SurfaceProvider
                }

                val texture = textureEntry!!.surfaceTexture()
                texture.setDefaultBufferSize(
                    request.resolution.width,
                    request.resolution.height
                )

                val surface = Surface(texture)
                request.provideSurface(surface, executor) { }
            }

            // Build the preview to be shown on the Flutter texture
            val previewBuilder = Preview.Builder()
            preview = previewBuilder.build().apply { setSurfaceProvider(surfaceProvider) }

            // Build the analyzer to be passed on to MLKit
            val analysisBuilder = ImageAnalysis.Builder()
                .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
            val displayManager = activity.applicationContext.getSystemService(Context.DISPLAY_SERVICE) as DisplayManager

            if (cameraResolution != null) {
                if (newCameraResolutionSelector) {
                    val selectorBuilder = ResolutionSelector.Builder()
                    selectorBuilder.setResolutionStrategy(
                        ResolutionStrategy(
                            cameraResolution,
                            ResolutionStrategy.FALLBACK_RULE_CLOSEST_HIGHER_THEN_LOWER
                        )
                    )
                    analysisBuilder.setResolutionSelector(selectorBuilder.build()).build()
                } else {
                    @Suppress("DEPRECATION")
                    analysisBuilder.setTargetResolution(getResolution(cameraResolution))
                }

                if (displayListener == null) {
                    displayListener = object : DisplayManager.DisplayListener {
                        override fun onDisplayAdded(displayId: Int) {}

                        override fun onDisplayRemoved(displayId: Int) {}

                        override fun onDisplayChanged(displayId: Int) {
                            if (newCameraResolutionSelector) {
                                val selectorBuilder = ResolutionSelector.Builder()
                                selectorBuilder.setResolutionStrategy(
                                    ResolutionStrategy(
                                        cameraResolution,
                                        ResolutionStrategy.FALLBACK_RULE_CLOSEST_HIGHER_THEN_LOWER
                                    )
                                )
                                analysisBuilder.setResolutionSelector(selectorBuilder.build()).build()
                            } else {
                                @Suppress("DEPRECATION")
                                analysisBuilder.setTargetResolution(getResolution(cameraResolution))
                            }
                        }
                    }

                    displayManager.registerDisplayListener(
                        displayListener, null,
                    )
                }
            }

            val analysis = analysisBuilder.build().apply { setAnalyzer(executor, captureOutput) }

            try {
                camera = cameraProvider?.bindToLifecycle(
                    activity as LifecycleOwner,
                    cameraPosition,
                    preview,
                    analysis
                )
                cameraSelector = cameraPosition
            } catch(exception: Exception) {
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
            }

            val resolution = analysis.resolutionInfo!!.resolution
            val width = resolution.width.toDouble()
            val height = resolution.height.toDouble()
            val portrait = (camera?.cameraInfo?.sensorRotationDegrees ?: 0) % 180 == 0

            // Start with 'unavailable' torch state.
            var currentTorchState: Int = -1

            camera?.cameraInfo?.let {
                if (!it.hasFlashUnit()) {
                    return@let
                }

                currentTorchState = it.torchState.value ?: -1
            }

            mobileScannerStartedCallback(
                MobileScannerStartParameters(
                    if (portrait) width else height,
                    if (portrait) height else width,
                    currentTorchState,
                    textureEntry!!.id(),
                    numberOfCameras ?: 0
                )
            )
        }, executor)

    }

    /**
     * Pause barcode scanning.
     */
    fun pause() {
        if (isPaused) {
            throw AlreadyPaused()
        } else if (isStopped()) {
            throw AlreadyStopped()
        }

        pauseCamera()
    }

    /**
     * Stop barcode scanning.
     */
    fun stop() {
        if (!isPaused && isStopped()) {
            throw AlreadyStopped()
        }

        releaseCamera()
    }

    private fun pauseCamera() {
        // Pause camera by unbinding all use cases
        cameraProvider?.unbindAll()
        isPaused = true
    }

    private fun resumeCamera() {
        // Resume camera by rebinding use cases
        cameraProvider?.let { provider ->
            val owner = activity as LifecycleOwner
            cameraSelector?.let { provider.bindToLifecycle(owner, it, preview) }
        }
        isPaused = false
    }

    private fun releaseCamera() {
        if (displayListener != null) {
            val displayManager = activity.applicationContext.getSystemService(Context.DISPLAY_SERVICE) as DisplayManager

            displayManager.unregisterDisplayListener(displayListener)
            displayListener = null
        }

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

        textureEntry?.release()
        textureEntry = null

        // Release the scanner.
        scanner?.close()
        scanner = null
        lastScanned = null
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
    @SuppressLint("UnsafeOptInUsageError")
    fun invertInputImage(imageProxy: ImageProxy): InputImage {
        val image = imageProxy.image ?: throw IllegalArgumentException("Image is null")

        // Convert YUV_420_888 image to NV21 format
        // based on our util helper
        val bitmap = Bitmap.createBitmap(image.width, image.height, Bitmap.Config.ARGB_8888)
        YuvToRgbConverter(activity).yuvToRgb(image, bitmap)

        // Invert RGB values
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
        val inputImage = InputImage.fromFilePath(activity, image)

        // Use a short lived scanner instance, which is closed when the analysis is done.
        val barcodeScanner: BarcodeScanner = barcodeScannerFactory(scannerOptions)

        barcodeScanner.process(inputImage).addOnSuccessListener { barcodes ->
            val barcodeMap = barcodes.map { barcode -> barcode.data }

            if (barcodeMap.isEmpty()) {
                onSuccess(null)
            } else {
                onSuccess(barcodeMap)
            }
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
