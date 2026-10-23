# Upstream integration

This branch combines the fork at `85130fad` with upstream's released 7.4.2
(`4ece0633`, upstream `master`). The common ancestor is `2c51a370`, so the merge
also includes upstream fixes released after the fork's 7.2.0 baseline.
Upstream `develop` has two additional example/CI changes; those are outside
this release-based integration.

## Preserved fork behavior

- ZXing-C++ remains the primary Android decoder, with ML Kit recovery.
- Native decoding, image enhancement options, extra barcode formats, and
  structured payload parsing remain available.
- Linux and Windows retain image/byte decoding through FFI; live camera
  capture is still unsupported there.
- Existing controller IDs and automatic camera resume behavior remain intact.
- Package version 8.0.0, `publish_to: none`, the Git platform-interface
  dependency, and the fork's dependency and SDK constraints are preserved.
- The fork's release workflow is retained. Upstream's repository-specific
  publishing and release automation is excluded. CI uses Flutter 3.47 and
  Java 21 to match the fork's requirements. The Android example uses AGP 9
  built-in Kotlin, with Java/Kotlin targets both set to 21 and Jetifier
  disabled. The legacy AGP DSL remains enabled for Flutter 3.47 compatibility.
  Explicit JVM targets avoid Kotlin inheriting JVM 25 from the host JDK while
  Java compilation targets JVM 21.

## Combined behavior

- Web supports native BarcodeDetector, ZXing-WASM, and legacy ZXing-JS, plus
  scan windows, preferred camera persistence, and improved camera constraints.
- Lens queries support a facing filter and the best close-range lens query.
- Android adopts upstream orientation, pause/restart, initialization failure,
  ProGuard, and frame release fixes. Both native ZXing and ML Kit paths share
  guarded frame cleanup, including asynchronous returned-image conversion.
  Consumer keep rules also preserve the custom engine's JNI class, constructor,
  and native method names during release shrinking.
- Apple adopts capture queue, buffer lifetime, and engine shutdown fixes while
  retaining the fork's payload parsing and optional native decoder.
- Disposing an inactive controller does not stop the active controller. It also
  removes the disposed controller from the fork's resume registry.
- Mirroring web barcode corners preserves structured payload fields.

## Barcode format compatibility

Upstream assigned numeric format IDs that conflict with existing fork IDs.
Existing fork IDs are unchanged throughout Dart, Android, Apple, and native C++.
The additional formats use unused bits:

| Format | Fork raw value |
| --- | ---: |
| Micro QR Code | 16777216 |
| GS1 DataBar Limited | 33554432 |

The C++ bridge maps both formats to the pinned ZXing-C++ engine. Apple Vision
supports GS1 DataBar Limited on iOS 15+/macOS 12+; Micro QR Code requires a
ZXing backend. Code that exchanges raw IDs with upstream must translate by
format name rather than assuming the two projects share numeric IDs.

## Future updates

Use the configured `upstream` remote. Fetch only the desired branch to avoid
importing upstream tags that collide with the fork's release tags:

```sh
git fetch upstream master
git switch -c integrate/upstream-next
git merge --no-commit --no-ff upstream/master
```

Resolve format mappings and controller lifecycle behavior explicitly. Run
Flutter analysis, library/example tests, web and Android builds, Android unit
tests, and native engine checks. Validate Apple builds on a macOS host and
exercise camera rotation, switching, scan windows, inversion, and recovery on
physical devices before shipping.

## Validation

- Flutter 3.47.7 / Dart 3.13.5: static analysis passes with no issues.
- Library tests: 395 passed. Example tests: 11 passed.
- Android plugin compilation and 56 unit tests pass (9 scanner/frame-release,
  36 lens selection, and 11 structured barcode tests). The JUnit adapter and
  launcher are declared explicitly for the updated Kotlin/Gradle setup.
- Web example Release build succeeds with the local scanner plugin registered.
- Native C++ Release build succeeds against pinned ZXing-C++ v2.3.0.
- Android debug APK assembly succeeds with `--target-platform android-arm64`.
  Both `libms_zxing.so` and `libms_zxing_jni.so` are packaged for ARM64. The
  plugin native build also compiles successfully for armeabi-v7a, x86, and x86_64.
- Native fixture checks decode ZXing's `microqrcode-1/1.png` and
  `databarltd-1/00.png` through the C ABI with the fork's new IDs, both with
  explicit filters and with default detection. A QR-only filter excludes both.
- iOS/macOS builds and physical-camera behavior require a macOS host/devices.


## Follow-up completeness work

See [IMPLEMENTATION_STATUS.md](IMPLEMENTATION_STATUS.md) for the current audit
status. Follow-up changes add structured FFI/web payloads, strict format
filtering, safe sized native decoding, worker isolates, image batches, Android
frame quality and Apple native build wiring. The CI matrix now includes native
Linux/Windows checks, Chrome regressions, Android release compilation and both
Apple dependency managers. DotCode and desktop live capture remain open features.
