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
        ByteBuffer luma, int width, int height, int rowStride, int formatMask,
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
        System.out.println("JNI Unicode, metadata and buffer bounds passed");
    }
}
