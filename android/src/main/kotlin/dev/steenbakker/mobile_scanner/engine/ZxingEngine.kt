package dev.steenbakker.mobile_scanner.engine

import java.nio.ByteBuffer

/**
 * Kotlin entry point for the native ZXing-C++ decoder.
 *
 * This is the primary decoder on the Android camera hot path. ML Kit is only
 * invoked as a recovery fallback when this engine repeatedly finds nothing (see
 * [dev.steenbakker.mobile_scanner.MobileScanner.captureOutput]).
 */
class ZxingEngine {

    /**
     * Decodes barcodes from a grayscale (luminance) plane.
     *
     * [luma] must be a direct [ByteBuffer] (e.g. the Y plane of a YUV_420_888
     * camera frame). [rowStride] is the plane's row stride in bytes.
     * [formatMask] is the OR-ed set of `BarcodeFormat.rawValue` bits to look for
     * (0 = all). The crop rectangle restricts scanning to a scan window in image
     * pixels; pass all zeros to scan the whole frame.
     */
    fun decodeLuma(
        luma: ByteBuffer,
        width: Int,
        height: Int,
        rowStride: Int,
        formatMask: Int,
        cropLeft: Int = 0,
        cropTop: Int = 0,
        cropWidth: Int = 0,
        cropHeight: Int = 0,
        tryHarder: Boolean = false,
        tryRotate: Boolean = true,
        tryInvert: Boolean = false,
        tryDownscale: Boolean = false,
        maxSymbols: Int = 0,
    ): List<ZxingBarcode> {
        if (!luma.isDirect) {
            return emptyList()
        }
        val results = nativeDecodeLuma(
            luma, width, height, rowStride, formatMask,
            cropLeft, cropTop, cropWidth, cropHeight,
            tryHarder, tryRotate, tryInvert, tryDownscale, maxSymbols,
        )
        return results?.toList() ?: emptyList()
    }

    private external fun nativeDecodeLuma(
        luma: ByteBuffer,
        width: Int,
        height: Int,
        rowStride: Int,
        formatMask: Int,
        cropLeft: Int,
        cropTop: Int,
        cropWidth: Int,
        cropHeight: Int,
        tryHarder: Boolean,
        tryRotate: Boolean,
        tryInvert: Boolean,
        tryDownscale: Boolean,
        maxSymbols: Int,
    ): Array<ZxingBarcode>?

    /** Returns the linked zxing-cpp version string. */
    external fun nativeVersion(): String

    companion object {
        @Volatile
        private var loaded = false

        /**
         * Loads the native library. Returns true on success; callers should
         * fall back to ML Kit if this returns false (e.g. unsupported ABI).
         */
        fun ensureLoaded(): Boolean {
            if (loaded) return true
            return try {
                System.loadLibrary("ms_zxing_jni")
                loaded = true
                true
            } catch (_: Throwable) {
                false
            }
        }
    }
}
