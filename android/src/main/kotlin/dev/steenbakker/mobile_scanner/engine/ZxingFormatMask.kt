package dev.steenbakker.mobile_scanner.engine

/** Keeps both int32 and int64 StandardMessageCodec format values intact. */
internal object ZxingFormatMask {
    fun fromFormats(formats: List<Number>?): Long {
        if (formats.isNullOrEmpty() || formats.any { it.toLong() == 0L }) return 0L
        val mask = formats.fold(0L) { acc, value ->
            val raw = value.toLong()
            acc or when (raw) {
                -1L -> 0L
                126L, 127L -> 128L
                else -> raw
            }
        }
        // An unknown-only request must not become the engine's all-formats 0.
        return if (mask == 0L) Long.MIN_VALUE else mask
    }
}
