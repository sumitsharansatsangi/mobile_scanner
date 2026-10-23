package dev.steenbakker.mobile_scanner.engine;

import java.awt.image.BufferedImage;
import java.io.File;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import javax.imageio.ImageIO;

/** Exercises the Android JNI implementation on a host JVM, without a camera. */
public final class ZxingEngine {
    private native ZxingBarcode[] nativeDecodeLuma(
        ByteBuffer luma, int width, int height, int rowStride, long formatMask,
        int cropLeft, int cropTop, int cropWidth, int cropHeight,
        boolean tryHarder, boolean tryRotate, boolean tryInvert,
        boolean tryDownscale, int maxSymbols);

    public static void main(String[] args) throws Exception {
        System.load(args[0]);
        BufferedImage image = ImageIO.read(new File(args[1]));
        ByteBuffer pixels = ByteBuffer.allocateDirect(image.getWidth() * image.getHeight());
        for (int y = 0; y < image.getHeight(); y++) {
            for (int x = 0; x < image.getWidth(); x++) {
                pixels.put((byte) (image.getRGB(x, y) & 255));
            }
        }
        ZxingEngine engine = new ZxingEngine();
        ZxingBarcode[] results = engine.nativeDecodeLuma(
            pixels, image.getWidth(), image.getHeight(), 0, 256,
            0, 0, 0, 0, true, true, true, true, 0);
        String expected = "Scanner \u2713 \u4e2d\u6587 \ud83d\ude00";
        if (results.length != 1 || !expected.equals(results[0].text)
            || !Arrays.equals(expected.getBytes(StandardCharsets.UTF_8), results[0].rawBytes)) {
            throw new AssertionError("JNI did not retain standard UTF-8 text and bytes");
        }
        if (!"QRCodeModel2".equals(results[0].metadata.get("formatName"))
            || !"]Q1".equals(results[0].metadata.get("symbologyIdentifier"))) {
            throw new AssertionError("JNI dropped decoder metadata");
        }
        if (engine.nativeDecodeLuma(ByteBuffer.allocateDirect(1), 240, 240, 0,
            256, 0, 0, 0, 0, true, true, true, true, 0).length != 0) {
            throw new AssertionError("JNI accepted an undersized image buffer");
        }
        for (int fixture = 0; fixture < 2; fixture++) {
            BufferedImage postal = ImageIO.read(new File(args[2 + fixture]));
            ByteBuffer postalPixels = ByteBuffer.allocateDirect(postal.getWidth() * postal.getHeight());
            for (int y = 0; y < postal.getHeight(); y++) {
                for (int x = 0; x < postal.getWidth(); x++) {
                    postalPixels.put((byte) (postal.getRGB(x, y) & 255));
                }
            }
            long format = fixture == 0 ? 2147483648L : 4294967296L;
            String payload = fixture == 0 ? "12345" : "40123456789";
            for (long mask : new long[] {format, 0, format | 256L}) {
                ZxingBarcode[] found = engine.nativeDecodeLuma(
                    postalPixels, postal.getWidth(), postal.getHeight(), 0, mask,
                    0, 0, 0, 0, true, true, true, true, 0);
                if (found.length != 1 || found[0].format != format ||
                    !payload.equals(found[0].text) ||
                    !Arrays.equals(payload.getBytes(StandardCharsets.UTF_8), found[0].rawBytes)) {
                    throw new AssertionError("JNI truncated postal format or payload for " + mask);
                }
                String name = fixture == 0 ? "POSTNET" : "PLANET";
                if (!name.equals(found[0].metadata.get("formatName"))) {
                    throw new AssertionError("JNI lost postal metadata");
                }
            }
        }
        System.out.println("JNI Unicode, metadata, buffer bounds and 64-bit postal formats passed");
    }
}
