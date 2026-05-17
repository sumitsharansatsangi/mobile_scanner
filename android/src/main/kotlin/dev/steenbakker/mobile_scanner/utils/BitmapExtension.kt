package dev.steenbakker.mobile_scanner.utils

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.ColorMatrix
import android.graphics.ColorMatrixColorFilter
import android.graphics.Matrix
import android.graphics.Paint
import android.graphics.Rect
import androidx.core.graphics.createBitmap
import kotlin.math.max
import kotlin.math.min

// Efficiently invert bitmap colors using ColorMatrix
fun invertBitmapColors(bitmap: Bitmap): Bitmap {
    val colorMatrix = ColorMatrix().apply {
        set(floatArrayOf(
            -1f, 0f, 0f, 0f, 255f,  // Red
            0f, -1f, 0f, 0f, 255f,  // Green
            0f, 0f, -1f, 0f, 255f,  // Blue
            0f, 0f, 0f, 1f, 0f      // Alpha
        ))
    }
    val paint = Paint().apply { colorFilter = ColorMatrixColorFilter(colorMatrix) }

    val invertedBitmap = createBitmap(bitmap.width, bitmap.height, bitmap.config!!)
    val canvas = Canvas(invertedBitmap)
    canvas.drawBitmap(bitmap, 0f, 0f, paint)

    return invertedBitmap
}

fun rotateBitmap(bitmap: Bitmap, rotationDegrees: Int): Bitmap {
    val matrix = Matrix()
    matrix.postRotate(rotationDegrees.toFloat())
    return Bitmap.createBitmap(
        bitmap, 0, 0, bitmap.getWidth(), bitmap.getHeight(), matrix,
        true
    )
}

// Enhanced image processing functions for better barcode detection

/**
 * Enhance contrast for better barcode detection
 */
fun enhanceContrast(bitmap: Bitmap, contrast: Float = 1.5f): Bitmap {
    val colorMatrix = ColorMatrix().apply {
        set(floatArrayOf(
            contrast, 0f, 0f, 0f, 0f,
            0f, contrast, 0f, 0f, 0f,
            0f, 0f, contrast, 0f, 0f,
            0f, 0f, 0f, 1f, 0f
        ))
    }
    val paint = Paint().apply { colorFilter = ColorMatrixColorFilter(colorMatrix) }

    val enhancedBitmap = createBitmap(bitmap.width, bitmap.height, bitmap.config!!)
    val canvas = Canvas(enhancedBitmap)
    canvas.drawBitmap(bitmap, 0f, 0f, paint)

    return enhancedBitmap
}

/**
 * Apply sharpening filter for clearer barcode edges
 */
fun sharpenBitmap(bitmap: Bitmap): Bitmap {
    val width = bitmap.width
    val height = bitmap.height
    val pixels = IntArray(width * height)
    bitmap.getPixels(pixels, 0, width, 0, 0, width, height)

    val sharpenedPixels = IntArray(width * height)

    for (y in 1 until height - 1) {
        for (x in 1 until width - 1) {
            val idx = y * width + x

            // Simple sharpening kernel
            val center = pixels[idx]
            val top = pixels[(y - 1) * width + x]
            val bottom = pixels[(y + 1) * width + x]
            val left = pixels[y * width + (x - 1)]
            val right = pixels[y * width + (x + 1)]

            // Sharpening formula: center * 5 - adjacent pixels
            val sharpened = center * 5 - top - bottom - left - right

            // Clamp values
            val r = min(255, max(0, (sharpened shr 16) and 0xFF))
            val g = min(255, max(0, (sharpened shr 8) and 0xFF))
            val b = min(255, max(0, sharpened and 0xFF))

            sharpenedPixels[idx] = (0xFF shl 24) or (r shl 16) or (g shl 8) or b
        }
    }

    val sharpenedBitmap = createBitmap(width, height, bitmap.config!!)
    sharpenedBitmap.setPixels(sharpenedPixels, 0, width, 0, 0, width, height)

    return sharpenedBitmap
}

/**
 * Crop bitmap to specified rectangle
 */
fun cropBitmap(bitmap: Bitmap, rect: Rect): Bitmap {
    val croppedWidth = rect.width()
    val croppedHeight = rect.height()

    return Bitmap.createBitmap(
        bitmap,
        max(0, rect.left),
        max(0, rect.top),
        min(croppedWidth, bitmap.width - rect.left),
        min(croppedHeight, bitmap.height - rect.top)
    )
}

/**
 * Resize bitmap maintaining aspect ratio
 */
fun resizeBitmap(bitmap: Bitmap, maxWidth: Int, maxHeight: Int): Bitmap {
    val ratio = min(maxWidth.toFloat() / bitmap.width, maxHeight.toFloat() / bitmap.height)
    val newWidth = (bitmap.width * ratio).toInt()
    val newHeight = (bitmap.height * ratio).toInt()

    return Bitmap.createScaledBitmap(bitmap, newWidth, newHeight, true)
}

/**
 * Convert bitmap to grayscale for better processing
 */
fun toGrayscale(bitmap: Bitmap): Bitmap {
    val grayscaleBitmap = createBitmap(bitmap.width, bitmap.height, Bitmap.Config.ARGB_8888)
    val canvas = Canvas(grayscaleBitmap)
    val paint = Paint()

    val colorMatrix = ColorMatrix().apply {
        setSaturation(0f)
    }
    paint.colorFilter = ColorMatrixColorFilter(colorMatrix)
    canvas.drawBitmap(bitmap, 0f, 0f, paint)

    return grayscaleBitmap
}

/**
 * Apply bilateral filter for noise reduction while preserving edges
 */
fun applyBilateralFilter(bitmap: Bitmap, sigmaSpace: Float = 2f, sigmaColor: Float = 0.1f): Bitmap {
    // Simplified bilateral filter implementation
    // In production, consider using RenderScript or native code for performance
    val width = bitmap.width
    val height = bitmap.height
    val pixels = IntArray(width * height)
    bitmap.getPixels(pixels, 0, width, 0, 0, width, height)

    val filteredPixels = IntArray(width * height)

    for (y in 0 until height) {
        for (x in 0 until width) {
            var sumR = 0.0
            var sumG = 0.0
            var sumB = 0.0
            var weightSum = 0.0

            // Sample neighboring pixels
            for (dy in -2..2) {
                for (dx in -2..2) {
                    val nx = x + dx
                    val ny = y + dy

                    if (nx in 0 until width && ny in 0 until height) {
                        val neighborPixel = pixels[ny * width + nx]
                        val centerPixel = pixels[y * width + x]

                        // Spatial weight
                        val spatialWeight = Math.exp(-(dx * dx + dy * dy) / (2.0 * sigmaSpace * sigmaSpace))

                        // Color weight
                        val nr = (neighborPixel shr 16) and 0xFF
                        val ng = (neighborPixel shr 8) and 0xFF
                        val nb = neighborPixel and 0xFF
                        val cr = (centerPixel shr 16) and 0xFF
                        val cg = (centerPixel shr 8) and 0xFF
                        val cb = centerPixel and 0xFF

                        val colorDiff = (nr - cr) * (nr - cr) + (ng - cg) * (ng - cg) + (nb - cb) * (nb - cb)
                        val colorWeight = Math.exp(-colorDiff / (2.0 * sigmaColor * sigmaColor))

                        val weight = spatialWeight * colorWeight

                        sumR += nr * weight
                        sumG += ng * weight
                        sumB += nb * weight
                        weightSum += weight
                    }
                }
            }

            val r = (sumR / weightSum).toInt().coerceIn(0, 255)
            val g = (sumG / weightSum).toInt().coerceIn(0, 255)
            val b = (sumB / weightSum).toInt().coerceIn(0, 255)

            filteredPixels[y * width + x] = (0xFF shl 24) or (r shl 16) or (g shl 8) or b
        }
    }

    val filteredBitmap = createBitmap(width, height, bitmap.config!!)
    filteredBitmap.setPixels(filteredPixels, 0, width, 0, 0, width, height)

    return filteredBitmap
}