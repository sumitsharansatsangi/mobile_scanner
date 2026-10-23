package dev.steenbakker.mobile_scanner.engine

import dev.steenbakker.mobile_scanner.objects.BarcodeFormats
import org.junit.Assert.*
import org.junit.Test

class ZxingFormatMaskTest {
    @Test fun mixedCodecIntegersRetainPostalBits() {
        assertEquals(6442451200L,
            ZxingFormatMask.fromFormats(listOf<Number>(256, 2147483648L, 4294967296L)))
        assertNull(BarcodeFormats.fromRawValueOrNull(4294967296L))
        assertNull(BarcodeFormats.fromRawValueOrNull(4294967297L))
    }

    @Test fun allAndUnknownHaveDistinctSemantics() {
        assertEquals(0L, ZxingFormatMask.fromFormats(null))
        assertEquals(0L, ZxingFormatMask.fromFormats(emptyList()))
        assertEquals(0L, ZxingFormatMask.fromFormats(listOf(0, 4294967296L)))
        assertEquals(Long.MIN_VALUE, ZxingFormatMask.fromFormats(listOf(-1)))
        assertEquals(4294967296L, ZxingFormatMask.fromFormats(listOf(-1, 4294967296L)))
    }

    @Test fun legacyItfAliasesStillNormalize() {
        assertEquals(4294967424L, ZxingFormatMask.fromFormats(listOf(126, 127, 4294967296L)))
        assertEquals(126L, ZxingFormatMask.fromFormats(listOf(2, 4, 8, 16, 32, 64)))
    }
}
