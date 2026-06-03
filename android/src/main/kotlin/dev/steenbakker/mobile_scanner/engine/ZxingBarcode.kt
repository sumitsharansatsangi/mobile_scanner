package dev.steenbakker.mobile_scanner.engine

/**
 * A single barcode decoded by the native ZXing-C++ engine.
 *
 * Instances are constructed from JNI (see ms_zxing_jni.cpp); the field order
 * and types must match the constructor signature used there.
 *
 * @param format the [dev.steenbakker.mobile_scanner.objects.BarcodeFormats]
 *   raw value (matches the Dart `BarcodeFormat.rawValue`).
 * @param text the decoded UTF-8 text, or null when unavailable.
 * @param rawBytes the raw decoded bytes, or null when unavailable.
 * @param corners 8 floats in full-image pixel coordinates, ordered
 *   [topLeftX, topLeftY, topRightX, topRightY, bottomRightX, bottomRightY,
 *   bottomLeftX, bottomLeftY].
 */
class ZxingBarcode(
    @JvmField val format: Int,
    @JvmField val text: String?,
    @JvmField val rawBytes: ByteArray?,
    @JvmField val corners: FloatArray,
)
