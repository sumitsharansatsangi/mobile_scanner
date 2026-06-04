package dev.steenbakker.mobile_scanner.utils

import android.content.Context
import android.graphics.Bitmap
import com.google.android.gms.tasks.Task
import com.google.android.gms.tasks.Tasks
import com.google.mlkit.vision.barcode.BarcodeScanner
import com.google.mlkit.vision.barcode.BarcodeScannerOptions
import com.google.mlkit.vision.barcode.BarcodeScanning
import com.google.mlkit.vision.barcode.common.Barcode
import com.google.mlkit.vision.common.InputImage
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/**
 * Advanced barcode processing utilities for enhanced detection
 */
class AdvancedBarcodeProcessor(private val context: Context) {

    /**
     * Process image with multiple enhancement techniques for better detection
     */
    suspend fun processWithEnhancements(
        bitmap: Bitmap,
        scannerOptions: BarcodeScannerOptions
    ): List<Barcode> = withContext(Dispatchers.Default) {
        val scanner = BarcodeScanning.getClient(scannerOptions)
        val results = mutableListOf<Barcode>()

        try {
            // Try original image first
            val originalImage = InputImage.fromBitmap(bitmap, 0)
            val originalResults = Tasks.await(scanner.process(originalImage))
            results.addAll(originalResults)

            // If no results, try enhanced versions
            if (results.isEmpty()) {
                // Try inverted colors
                val invertedBitmap = invertBitmapColors(bitmap)
                val invertedImage = InputImage.fromBitmap(invertedBitmap, 0)
                val invertedResults = Tasks.await(scanner.process(invertedImage))
                results.addAll(invertedResults)
                invertedBitmap.recycle()

                // Try enhanced contrast
                if (results.isEmpty()) {
                    val contrastBitmap = enhanceContrast(bitmap, 2.0f)
                    val contrastImage = InputImage.fromBitmap(contrastBitmap, 0)
                    val contrastResults = Tasks.await(scanner.process(contrastImage))
                    results.addAll(contrastResults)
                    contrastBitmap.recycle()
                }

                // Try sharpened image
                if (results.isEmpty()) {
                    val sharpenedBitmap = sharpenBitmap(bitmap)
                    val sharpenedImage = InputImage.fromBitmap(sharpenedBitmap, 0)
                    val sharpenedResults = Tasks.await(scanner.process(sharpenedImage))
                    results.addAll(sharpenedResults)
                    sharpenedBitmap.recycle()
                }

                // Try grayscale
                if (results.isEmpty()) {
                    val grayscaleBitmap = toGrayscale(bitmap)
                    val grayscaleImage = InputImage.fromBitmap(grayscaleBitmap, 0)
                    val grayscaleResults = Tasks.await(scanner.process(grayscaleImage))
                    results.addAll(grayscaleResults)
                    grayscaleBitmap.recycle()
                }
            }
        } finally {
            scanner.close()
        }

        results
    }

    /**
     * Create scanner options with all available features enabled
     */
    fun createAdvancedScannerOptions(formats: List<Int>? = null): BarcodeScannerOptions {
        val builder = BarcodeScannerOptions.Builder()

        // Enable all formats if none specified
        if (formats.isNullOrEmpty()) {
            builder.setBarcodeFormats(Barcode.FORMAT_ALL_FORMATS)
        } else {
            val supportedFormats = formats.filter {
                it == Barcode.FORMAT_UNKNOWN ||
                it == Barcode.FORMAT_ALL_FORMATS ||
                it == Barcode.FORMAT_CODE_128 ||
                it == Barcode.FORMAT_CODE_39 ||
                it == Barcode.FORMAT_CODE_93 ||
                it == Barcode.FORMAT_CODABAR ||
                it == Barcode.FORMAT_DATA_MATRIX ||
                it == Barcode.FORMAT_EAN_13 ||
                it == Barcode.FORMAT_EAN_8 ||
                it == Barcode.FORMAT_ITF ||
                it == Barcode.FORMAT_QR_CODE ||
                it == Barcode.FORMAT_UPC_A ||
                it == Barcode.FORMAT_UPC_E ||
                it == Barcode.FORMAT_PDF417 ||
                it == Barcode.FORMAT_AZTEC
            }
            val mlKitFormats = supportedFormats.ifEmpty {
                listOf(Barcode.FORMAT_UNKNOWN)
            }
            builder.setBarcodeFormats(
                mlKitFormats.first(),
                *mlKitFormats.subList(1, mlKitFormats.size).toIntArray()
            )
        }

        // Enable all potential barcodes (new feature)
        builder.enableAllPotentialBarcodes()

        return builder.build()
    }

    /**
     * Batch process multiple images concurrently
     */
    suspend fun batchProcess(
        bitmaps: List<Bitmap>,
        scannerOptions: BarcodeScannerOptions
    ): List<List<Barcode>> = withContext(Dispatchers.Default) {
        bitmaps.map { bitmap ->
            processWithEnhancements(bitmap, scannerOptions)
        }
    }

    /**
     * Analyze image quality for barcode detection potential
     */
    fun analyzeImageQuality(bitmap: Bitmap): ImageQualityAnalysis {
        val width = bitmap.width
        val height = bitmap.height
        val pixels = IntArray(width * height)
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height)

        var totalBrightness = 0f
        var totalContrast = 0f
        var edgeCount = 0

        // Sample pixels for analysis
        val sampleSize = minOf(100, pixels.size)
        val step = pixels.size / sampleSize

        val sampledPixels = (0 until pixels.size step step).map { pixels[it] }

        for (i in 1 until sampledPixels.size) {
            val pixel1 = sampledPixels[i - 1]
            val pixel2 = sampledPixels[i]

            // Calculate brightness
            val brightness1 = ((pixel1 shr 16) and 0xFF) * 0.299f +
                             ((pixel1 shr 8) and 0xFF) * 0.587f +
                             (pixel1 and 0xFF) * 0.114f
            val brightness2 = ((pixel2 shr 16) and 0xFF) * 0.299f +
                             ((pixel2 shr 8) and 0xFF) * 0.587f +
                             (pixel2 and 0xFF) * 0.114f

            totalBrightness += brightness1

            // Calculate contrast (edge detection)
            val contrast = kotlin.math.abs(brightness1 - brightness2)
            totalContrast += contrast

            if (contrast > 30) edgeCount++
        }

        val avgBrightness = totalBrightness / sampledPixels.size
        val avgContrast = totalContrast / sampledPixels.size
        val edgeDensity = edgeCount.toFloat() / sampledPixels.size

        return ImageQualityAnalysis(
            brightness = avgBrightness / 255f, // Normalize to 0-1
            contrast = avgContrast / 255f,
            edgeDensity = edgeDensity,
            recommendedEnhancement = when {
                avgBrightness < 80 -> "increase_brightness"
                avgContrast < 20 -> "increase_contrast"
                edgeDensity < 0.1 -> "sharpen"
                else -> "none"
            }
        )
    }
}

/**
 * Image quality analysis result
 */
data class ImageQualityAnalysis(
    val brightness: Float, // 0-1 scale
    val contrast: Float,   // 0-1 scale
    val edgeDensity: Float, // 0-1 scale
    val recommendedEnhancement: String
)
