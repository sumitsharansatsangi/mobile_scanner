package dev.steenbakker.mobile_scanner.engine;

import java.util.Map;

/** Host-side equivalent of the Kotlin object constructed by the real JNI bridge. */
public final class ZxingBarcode {
    public final int format;
    public final String text;
    public final byte[] rawBytes;
    public final float[] corners;
    public Map<String, Object> metadata;

    public ZxingBarcode(int format, String text, byte[] rawBytes, float[] corners) {
        this.format = format;
        this.text = text;
        this.rawBytes = rawBytes;
        this.corners = corners;
    }
}
