package dev.steenbakker.mobile_scanner.utils

import java.nio.ByteBuffer
import kotlin.math.max
import kotlin.math.sqrt

/** Cheap, bounded sampling of the Y plane; no bitmap allocation or mutation. */
internal object FrameQuality {
    fun measure(data: ByteBuffer, width: Int, height: Int, rowStride: Int, pixelStride: Int): Map<String, Double>? {
        if (width <= 0 || height <= 0 || rowStride <= 0 || pixelStride <= 0) return null
        val buffer = data.duplicate()
        val offset = buffer.position()
        val required = (height - 1).toLong() * rowStride + (width - 1).toLong() * pixelStride + 1
        if (required > buffer.remaining()) return null
        var count = 0
        var sum = 0.0
        var squares = 0.0
        var edges = 0.0
        val stepX = max(1, width / 64)
        val stepY = max(1, height / 64)
        for (y in 0 until height step stepY) {
            for (x in 0 until width step stepX) {
                val index = offset + y * rowStride + x * pixelStride
                val value = buffer.get(index).toInt() and 255
                sum += value
                squares += value.toDouble() * value
                if (x + 1 < width) {
                    edges += kotlin.math.abs(value - (buffer.get(index + pixelStride).toInt() and 255))
                }
                count++
            }
        }
        val mean = sum / count
        return mapOf(
            "brightness" to mean / 255.0,
            "contrast" to (sqrt(max(0.0, squares / count - mean * mean)) / 127.5).coerceIn(0.0, 1.0),
            "sharpness" to edges / count / 255.0,
        )
    }
}
