# Barcode format audit

Audited against the bundled ZXing-C++ **2.3.0**, ZXing-WASM **3.1.3** and
legacy ZXing-js **0.23.0**. Format availability depends on the selected engine;
ML Kit and Apple Vision do not implement the complete native ZXing format set.

## Fixes in this audit

- Exposed DX film edge end to end through `BarcodeFormat.dxFilmEdge`.
- Added WASM selectors for Telepen, PZN, Code 32, DX film edge, the EAN/UPC
  family and the engine's other-barcode family. Previous Code 32 and PZN
  output was flattened into Code 39; Telepen and DX output became unknown.
- Preserved exact variant names, AIM identifiers, version, error correction,
  inversion/mirror flags and structured-append information in optional
  `Barcode.metadata`. Existing C result/list layouts and enum IDs are preserved.
- Fixed valid combined masks **126/127** being misread as legacy ITF aliases.
  Public bindings normalize legacy ITF aliases to bit 128 before forming masks.
- Enforced GS1-only requests and recognized GS1-128 on the web. Code 128 parent
  requests still include GS1-128. Legacy JS enables its FNC1 conversion hint.
- Fixed ITF alias matching between web and native readers. Checksum-only ITF
  requests now validate Mod10 against the full returned numeric value.
- Enabled web EAN/UPC supplements with the correct reader options and retail
  readers. Normalized addon text to `main supplement`, matching native output.
  EAN-2 and EAN-5 are supplements, not independently readable WASM formats.
- Web auto/all now chooses WASM to retain broad coverage. An explicit list
  uses the native browser detector only if it supports every requested format.
- WASM now honors the inverted-image scan options.
- Removed the wrapper's arbitrary 16-symbol default; the native default is 255.
- Fixed Android JNI's modified-UTF8 conversion, which corrupted emoji.

## Complete bundled native format inventory

Every concrete format in the vendored `BarcodeFormat.h` has a fixture and is
covered by explicit-format, all-format and exclusion tests through Dart FFI.
The fixture manifest records original upstream paths and expected text.

| ZXing-C++ format | Public format | Fixture |
| --- | --- | --- |
| Aztec | `aztec` | [aztec.png](test/fixtures/formats/aztec.png) |
| Codabar | `codabar` | [codabar.png](test/fixtures/formats/codabar.png) |
| Code39 | `code39` | [code39.png](test/fixtures/formats/code39.png) |
| Code93 | `code93` | [code93.png](test/fixtures/formats/code93.png) |
| Code128 | `code128` | [code128.png](test/fixtures/formats/code128.png) |
| DataBar | `dataBar` | [dataBar.png](test/fixtures/formats/dataBar.png) |
| DataBarExpanded | `dataBarExpanded` | [dataBarExpanded.png](test/fixtures/formats/dataBarExpanded.png) |
| DataMatrix | `dataMatrix` | [dataMatrix.png](test/fixtures/formats/dataMatrix.png) |
| EAN8 | `ean8` | [ean8.png](test/fixtures/formats/ean8.png) |
| EAN13 | `ean13` | [ean13.png](test/fixtures/formats/ean13.png) |
| ITF | `itf14` | [itf14.png](test/fixtures/formats/itf14.png) |
| MaxiCode | `maxiCode` | [maxiCode.png](test/fixtures/formats/maxiCode.png) |
| PDF417 | `pdf417` | [pdf417.png](test/fixtures/formats/pdf417.png) |
| QRCode | `qrCode` | [qrCode.png](test/fixtures/formats/qrCode.png) |
| UPCA | `upcA` | [upcA.png](test/fixtures/formats/upcA.png) |
| UPCE | `upcE` | [upcE.png](test/fixtures/formats/upcE.png) |
| MicroQRCode | `microQrCode` | [microQrCode.png](test/fixtures/formats/microQrCode.png) |
| RMQRCode | `rmQrCode` | [rmQrCode.png](test/fixtures/formats/rmQrCode.png) |
| DXFilmEdge | `dxFilmEdge` | [dxFilmEdge.png](test/fixtures/formats/dxFilmEdge.png) |
| DataBarLimited | `dataBarLimited` | [dataBarLimited.png](test/fixtures/formats/dataBarLimited.png) |

Additional fixtures cover QR Model 1, GS1-128, EAN-5 supplements, Unicode,
20 simultaneous QR symbols, and small/large/rotated/inverted rMQR symbols.

## Complete WASM readable-name inventory

All **41 readable names** from the pinned upstream table map to a known public
format. Subvariants share public families; `metadata.formatName` preserves the
engine's exact name, with QR Model 1 also identified from `]Q0` when necessary.
These names include family selectors as well as concrete variants; they are not
41 independent physical symbologies.

| WASM names | Public format |
| --- | --- |
| Codabar | `codabar` |
| Code39, Code39Std, Code39Ext | `code39` |
| Code32; PZN | `code32`; `pzn` |
| Code93; Code128 | `code93`; `code128` or `gs1Code128` from AIM identifier |
| ITF; ITF14 | `itf`; `itf14` |
| DataBar, DataBarOmni, DataBarStk, DataBarStkOmni | `dataBar` |
| DataBarLtd | `dataBarLimited` |
| DataBarExp, DataBarExpStk | `dataBarExpanded` |
| EANUPC | `eanUpc` |
| EAN13, ISBN; EAN8; UPCA; UPCE | `ean13`; `ean8`; `upcA`; `upcE` |
| Telepen, TelepenAlpha, TelepenNumeric | `telepen` |
| OtherBarcode; DXFilmEdge | `otherBarcode`; `dxFilmEdge` |
| PDF417, CompactPDF417, MicroPDF417 | `pdf417` |
| Aztec, AztecCode, AztecRune | `aztec` |
| QRCode, QRCodeModel1, QRCodeModel2 | `qrCode` |
| MicroQRCode; RMQRCode | `microQrCode`; `rmQrCode` |
| DataMatrix; MaxiCode | `dataMatrix`; `maxiCode` |

Compatibility aliases such as `rMQRCode`, `DataBarExpanded`, `DataBarLimited`
and older hyphenated EAN/UPC names remain accepted. Upstream meta selectors
(AllLinear, AllMatrix, AllGS1, etc.) are configuration groups, not additional
barcode formats; applications can request lists of public formats.

## Remaining engine and API boundaries

- **Telepen and distinct PZN decoding are WASM-only.** Native 2.3.0 has no
  Telepen reader or distinct PZN reader. A PZN's underlying Code 39 can be read
  with `code39`; requesting `pzn` does not silently substitute that format.
- **MicroPDF417 is WASM-only.** Native 2.3.0 does not implement this reader.
  On web select `pdf417`; inspect metadata to distinguish returned variants.
- Native QR Model 1/2, Micro QR and rMQR are supported. Micro QR and rMQR have
  separate selectors; an explicit `qrCode` request remains standard QR only.
- Native Aztec includes its reader's Rune support; the 2.3.0 result API reports
  the Aztec family, so it does not promise an exact Rune subtype name. Likewise
  native PDF417/DataBar subvariants retain only details the old engine reports.
- Native `itf14` retains the historical generic ITF bit for compatibility; it
  does not enforce 14 digits. WASM's explicit ITF14 selector is more specific.
  `itf2of5WithChecksum` checks Mod10 in the shared result filter; native C ABI
  callers receive generic ITF and must apply their own checksum policy.
- Code 11, MSI/Plessey and Pharmacode use this fork's native sampled-line
  fallbacks, not upstream ZXing readers. Their existing limitations remain.
  Pharmacode is explicit-only because it has no checksum.
- DotCode remains a reserved enum without an implemented reader in these
  pinned engines. Four-state postal symbologies, Han Xin, Grid Matrix, iQR and proprietary
  QR derivatives are not enabled merely by adding enum entries.
- Metadata is optional. ML Kit, Vision and native browser results may omit it.
  Structured-append pieces are exposed individually; automatic assembly is not
  implemented. Barcode payload parsing is separate from symbol decoding.
- Apple Swift/Xcode builds and physical Android live-camera scanning require
  their respective environments. Image/JVM tests do not establish camera focus,
  lighting, resolution or device performance. Desktop live camera is still absent.

## Verification

- 443 Flutter tests passed with native fixtures enabled, including all 20 native
  formats and mapping coverage for all 41 WASM readable names.
- The updated Apple vendored C++ bridge also compiled and linked on Linux;
  this does not validate Swift or Xcode.
- Native CTest passed: sized ABI bounds/strict formats plus actual Android JNI
  code running on a JVM with Unicode, raw bytes and metadata assertions.
- Eight Chrome conversion/backend regressions passed for backend selection,
  GS1, EAN add-ons and variant metadata.
- 59 Android JVM tests passed; Flutter analysis reported no issues.
- Android ARM64 release APK built successfully with application R8 enabled;
  the new metadata accessor and both native libraries are packaged.
- A separate local run of the real pinned WASM binary decoded QR Model 1,
  Micro QR, rMQR, DX film edge, GS1-128 and EAN supplements. The 41-name mapping
  check is not a claim of 41 independent real-image WASM fixture tests.

## Sources

- [Bundled native format definition](darwin/mobile_scanner/Sources/MobileScannerNative/zxing/BarcodeFormat.h)
- [ZXing-C++ 2.3.0 sample corpus](https://github.com/zxing-cpp/zxing-cpp/tree/v2.3.0/test/samples)
- [Pinned WASM readable format table](https://github.com/Sec-ant/zxing-wasm/blob/v3.1.3/src/bindings/barcodeFormat.ts)
- [Pinned WASM reader options](https://github.com/Sec-ant/zxing-wasm/blob/v3.1.3/src/bindings/readerOptions.ts)
- [Pinned legacy reader](https://github.com/zxing-js/library/blob/v0.23.0/src/core/MultiFormatReader.ts)

## POSTNET and PLANET follow-up

Two native sampled-line image readers now supplement the 20 bundled ZXing
formats. `BarcodeFormat.postnet` (2147483648) and `BarcodeFormat.planet`
(4294967296) are supported explicitly and in default/all detection. They require
valid frame bars, legal lengths, digit patterns and a Mod-10 checksum; output
omits the checksum. Direction follows the common baseline, preventing a failed
checksum from being retried backwards as a different valid payload.

Format masks and native result IDs now use 64-bit integers throughout Dart FFI,
Android JNI/Kotlin and Swift. Android accepts mixed 32/64-bit method-channel
integers without truncating high postal bits into existing formats. Existing
public format IDs are unchanged. The C ABI uses versioned decode symbols
`ms_zxing_decode_v2` and `ms_zxing_decode_sized_v2`; native binaries and bindings
must be rebuilt together. Old ABI binaries fail symbol lookup rather than
being interpreted using the new struct layout.

Synthetic image fixtures and their generator live in `test/fixtures/postal`.
They cover every supported payload length, upright/90/180-degree orientation,
inversion, bad checksums, missing frame bars and unsupported lengths. FFI tests
also check padded color buffers, cropping, corners, decoded bytes and strict
format requests. JNI tests exercise both high format bits on an actual JVM.
These do not establish physical-camera performance or Apple Swift build success.

Web, ML Kit and Apple Vision do not acquire postal decoding from this change.
The detector requires nearly axis-aligned symbols and clear scan lines; a scan
window can isolate postal bars from surrounding print. Four-state postal readers
remain unimplemented. See [postal usage](BARCODE_AND_QR_SUPPORT.md#postal-barcodes).

Follow-up validation: 451 Flutter tests (native fixtures enabled), 62 Android
JVM tests, eight Chrome regressions and both native/JNI CTests passed. Static
analysis was clean. The Apple vendored C++ source bridge also compiled and
linked on Linux; this does not validate Swift or Xcode.

The Android ARM64 release APK built successfully with application R8 enabled.
Both packaged native libraries reference the v2 decode ABI.
