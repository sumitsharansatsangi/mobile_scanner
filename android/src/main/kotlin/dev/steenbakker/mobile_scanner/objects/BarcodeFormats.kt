package dev.steenbakker.mobile_scanner.objects

enum class BarcodeFormats(val intValue: Int) {
    UNKNOWN(com.google.mlkit.vision.barcode.common.Barcode.FORMAT_UNKNOWN),
    ALL_FORMATS(com.google.mlkit.vision.barcode.common.Barcode.FORMAT_ALL_FORMATS),
    CODE_128(com.google.mlkit.vision.barcode.common.Barcode.FORMAT_CODE_128),
    CODE_39(com.google.mlkit.vision.barcode.common.Barcode.FORMAT_CODE_39),
    CODE_93(com.google.mlkit.vision.barcode.common.Barcode.FORMAT_CODE_93),
    CODABAR(com.google.mlkit.vision.barcode.common.Barcode.FORMAT_CODABAR),
    DATA_MATRIX(com.google.mlkit.vision.barcode.common.Barcode.FORMAT_DATA_MATRIX),
    EAN_13(com.google.mlkit.vision.barcode.common.Barcode.FORMAT_EAN_13),
    EAN_8(com.google.mlkit.vision.barcode.common.Barcode.FORMAT_EAN_8),
    EAN_UPC(1632),
    ITF(com.google.mlkit.vision.barcode.common.Barcode.FORMAT_ITF),
    QR_CODE(com.google.mlkit.vision.barcode.common.Barcode.FORMAT_QR_CODE),
    UPC_A(com.google.mlkit.vision.barcode.common.Barcode.FORMAT_UPC_A),
    UPC_E(com.google.mlkit.vision.barcode.common.Barcode.FORMAT_UPC_E),
    PDF417(com.google.mlkit.vision.barcode.common.Barcode.FORMAT_PDF417),
    AZTEC(com.google.mlkit.vision.barcode.common.Barcode.FORMAT_AZTEC);

    companion object {
        fun fromRawValueOrNull(rawValue: Long): BarcodeFormats? {
            return when(rawValue) {
                -1L -> UNKNOWN
                0L -> ALL_FORMATS
                1L -> CODE_128
                2L -> CODE_39
                8388608L -> CODE_39
                4L -> CODE_93
                8L -> CODABAR
                16L -> DATA_MATRIX
                32L -> EAN_13
                64L -> EAN_8
                1632L -> EAN_UPC
                126L, 127L, 128L -> ITF
                256L -> QR_CODE
                512L -> UPC_A
                1024L -> UPC_E
                2048L -> PDF417
                4096L -> AZTEC
                else -> null
            }
        }

        fun fromRawValue(rawValue: Int): BarcodeFormats {
            return fromRawValueOrNull(rawValue.toLong()) ?: UNKNOWN
        }
    }
}
