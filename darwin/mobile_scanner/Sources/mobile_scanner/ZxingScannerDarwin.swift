// The ZXing-C++ primary engine on Apple platforms is gated behind the
// MOBILE_SCANNER_ZXING compilation condition. It stays off until the native
// core (src/ms_zxing.{h,cpp}) and zxing-cpp are wired into the darwin build
// (see the setup notes in mobile_scanner.podspec). With the flag off, the
// plugin builds exactly as before and uses Apple Vision only.
#if MOBILE_SCANNER_ZXING

import CoreVideo
import Foundation

#if canImport(Flutter)
import Flutter
#elseif canImport(FlutterMacOS)
import FlutterMacOS
#endif

/// Bridges the shared ZXing-C++ C ABI (ms_zxing.h) to the Apple capture
/// pipeline. This is the primary decoder on iOS/macOS; Apple Vision is only
/// used as a recovery fallback (see `MobileScannerPlugin.captureOutput`).
///
/// The C functions `ms_zxing_decode` / `ms_zxing_free_results` are provided by
/// the native core compiled into this module (see mobile_scanner.podspec /
/// Package.swift). The result dictionaries match the shape produced by
/// `VNBarcodeObservation.toMap` so the Dart `Barcode.fromNative` parser handles
/// both engines identically.
enum ZxingScannerDarwin {

    // Pixel-format constants mirroring MsZxingPixelFormat in ms_zxing.h.
    private static let pixelLum: Int32 = 0
    private static let pixelBGRA: Int32 = 3

    /// Decode all barcodes in [pixelBuffer]. [formatMask] is the OR-ed set of
    /// `BarcodeFormat.rawValue` bits (0 = all). [crop] is an optional scan
    /// window in image pixels.
    static func decode(
        pixelBuffer: CVPixelBuffer,
        formatMask: UInt32,
        crop: CGRect? = nil,
        tryHarder: Bool = false
    ) -> [[String: Any?]] {
        CVPixelBufferLockBaseAddress(pixelBuffer, .readOnly)
        defer { CVPixelBufferUnlockBaseAddress(pixelBuffer, .readOnly) }

        let width = CVPixelBufferGetWidth(pixelBuffer)
        let height = CVPixelBufferGetHeight(pixelBuffer)
        if width <= 0 || height <= 0 {
            return []
        }

        // Prefer the luminance plane for planar (4:2:0) buffers, otherwise treat
        // the buffer as interleaved 32BGRA (the plugin's preferred format).
        let data: UnsafeMutableRawPointer?
        let rowStride: Int
        let msPixelFormat: Int32
        if CVPixelBufferIsPlanar(pixelBuffer) {
            data = CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0)
            rowStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0)
            msPixelFormat = pixelLum
        } else {
            data = CVPixelBufferGetBaseAddress(pixelBuffer)
            rowStride = CVPixelBufferGetBytesPerRow(pixelBuffer)
            msPixelFormat = pixelBGRA
        }

        guard let base = data else {
            return []
        }

        var params = MsZxingDecodeParams()
        memset(&params, 0, MemoryLayout<MsZxingDecodeParams>.size)
        params.data = base.assumingMemoryBound(to: UInt8.self)
        params.width = Int32(width)
        params.height = Int32(height)
        params.row_stride = Int32(rowStride)
        params.pixel_format = msPixelFormat
        params.format_mask = formatMask
        if let crop = crop {
            params.crop_left = Int32(crop.origin.x)
            params.crop_top = Int32(crop.origin.y)
            params.crop_width = Int32(crop.size.width)
            params.crop_height = Int32(crop.size.height)
        }
        params.try_harder = tryHarder ? 1 : 0
        params.try_rotate = 1
        params.try_invert = 0
        params.try_downscale = tryHarder ? 1 : 0
        params.max_symbols = 0

        guard let listPtr = ms_zxing_decode(&params) else {
            return []
        }
        defer { ms_zxing_free_results(listPtr) }

        let list = listPtr.pointee
        if list.count <= 0 || list.results == nil {
            return []
        }

        var output: [[String: Any?]] = []
        output.reserveCapacity(Int(list.count))

        for i in 0..<Int(list.count) {
            let result = list.results[i]

            var displayValue: String? = nil
            if let textPtr = result.text {
                displayValue = String(cString: textPtr)
            }

            var rawBytes: FlutterStandardTypedData? = nil
            if let bytesPtr = result.bytes, result.bytes_length > 0 {
                let buffer = Data(bytes: bytesPtr, count: Int(result.bytes_length))
                rawBytes = FlutterStandardTypedData(bytes: buffer)
            }

            // corners[0..3] = topLeft, topRight, bottomRight, bottomLeft.
            let c = result.corners
            let corners: [[String: CGFloat]] = [
                ["x": CGFloat(c.0.x), "y": CGFloat(c.0.y)],
                ["x": CGFloat(c.1.x), "y": CGFloat(c.1.y)],
                ["x": CGFloat(c.2.x), "y": CGFloat(c.2.y)],
                ["x": CGFloat(c.3.x), "y": CGFloat(c.3.y)],
            ]

            let xs = [c.0.x, c.1.x, c.2.x, c.3.x]
            let ys = [c.0.y, c.1.y, c.2.y, c.3.y]
            let sizeWidth = CGFloat((xs.max() ?? 0) - (xs.min() ?? 0))
            let sizeHeight = CGFloat((ys.max() ?? 0) - (ys.min() ?? 0))

            // Reuse the heuristic value-type detector used for the Vision path.
            let barcodeType = displayValue?.detectBarcodeType() ?? 0

            output.append([
                "corners": corners,
                "format": Int(result.format),
                "rawBytes": rawBytes,
                "rawPayloadData": nil,
                "rawValue": displayValue,
                "displayValue": displayValue,
                "size": ["width": sizeWidth, "height": sizeHeight],
                "type": barcodeType,
            ])
        }

        return output
    }
}

#endif // MOBILE_SCANNER_ZXING
