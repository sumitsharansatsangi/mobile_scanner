package dev.steenbakker.mobile_scanner

import dev.steenbakker.mobile_scanner.utils.FrameQuality
import java.nio.ByteBuffer
import org.junit.Assert.*
import org.junit.Test

class FrameQualityTest {
    @Test fun uniformFramesHaveNoContrastOrEdges() {
        val quality = FrameQuality.measure(ByteBuffer.wrap(ByteArray(16) { 128.toByte() }), 4, 4, 4, 1)!!
        assertEquals(128.0 / 255, quality["brightness"]!!, 0.00001)
        assertEquals(0.0, quality["contrast"]!!, 0.00001)
        assertEquals(0.0, quality["sharpness"]!!, 0.00001)
    }
    @Test fun handlesPaddingPixelStrideAndBufferPositionWithoutMutatingInput() {
        val buffer = ByteBuffer.wrap(byteArrayOf(99, 0, 88, -1, 88, 77, 77, 0, 88, -1))
        buffer.position(1)
        val quality = FrameQuality.measure(buffer, 2, 2, 6, 2)!!
        assertEquals(0.5, quality["brightness"]!!, 0.00001)
        assertEquals(1.0, quality["contrast"]!!, 0.00001)
        assertEquals(0.5, quality["sharpness"]!!, 0.00001)
        assertEquals(1, buffer.position())
    }
    @Test fun rejectsTruncatedPlanesAndInvalidDimensions() {
        assertNull(FrameQuality.measure(ByteBuffer.allocate(3), 2, 2, 2, 1))
        assertNull(FrameQuality.measure(ByteBuffer.allocate(4), 0, 2, 2, 1))
    }
}
