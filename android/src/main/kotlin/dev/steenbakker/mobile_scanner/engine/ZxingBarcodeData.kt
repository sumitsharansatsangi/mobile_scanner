package dev.steenbakker.mobile_scanner.engine

/**
 * Maps a [ZxingBarcode] to the same map shape that ML Kit barcodes are sent to
 * Flutter with (see `Barcode.data` in MobileScannerUtilities.kt), so the Dart
 * `Barcode.fromNative` parser handles both engines identically.
 *
 * Structured value types (calendarEvent, wifi, etc.) are not produced by the
 * ZXing engine and are therefore null; `type` is left unknown (0).
 */
val ZxingBarcode.data: Map<String, Any?>
    get() {
        val cornerList = listOf(
            mapOf("x" to corners[0].toDouble(), "y" to corners[1].toDouble()),
            mapOf("x" to corners[2].toDouble(), "y" to corners[3].toDouble()),
            mapOf("x" to corners[4].toDouble(), "y" to corners[5].toDouble()),
            mapOf("x" to corners[6].toDouble(), "y" to corners[7].toDouble()),
        )

        // Derive a bounding size from the corner points.
        val xs = listOf(corners[0], corners[2], corners[4], corners[6])
        val ys = listOf(corners[1], corners[3], corners[5], corners[7])
        val sizeMap = mapOf(
            "width" to (xs.max() - xs.min()).toDouble(),
            "height" to (ys.max() - ys.min()).toDouble(),
        )

        return mapOf(
            "calendarEvent" to null,
            "contactInfo" to null,
            "corners" to cornerList,
            "displayValue" to text,
            "driverLicense" to null,
            "email" to null,
            "format" to format,
            "geoPoint" to null,
            "phone" to null,
            "rawBytes" to rawBytes,
            "rawValue" to text,
            "size" to sizeMap,
            "sms" to null,
            "type" to 0, // BarcodeType.unknown; ZXing does not classify value types
            "url" to null,
            "wifi" to null,
        )
    }
