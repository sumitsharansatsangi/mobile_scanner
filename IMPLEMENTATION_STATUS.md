# Implementation status

This records the follow-up to the upstream integration audit. Statuses distinguish
implemented code from platform and hardware validation that has not run here.

| Audited gap | Current implementation | Remaining work |
| --- | --- | --- |
| FFI drops structured payloads | Shared Dart parsing for MECARD, vCard, WiFi, email, phone, SMS, URLs/bookmarks, geo, calendar and AAMVA; native SDK payloads preserved | More uncommon payload dialects can be added with fixtures |
| Requested formats widen to all | Output filtering on web and controllers, empty native-mask guard, no unsupported-only web decoder invocation, browser auto selection checks requested capabilities | Physical camera regression across fallback engines |
| DotCode falsely advertised | Support tables and enum documentation corrected; unsupported-only format regression | No DotCode decoder is implemented; a backend and corpus are required |
| Quality switch does nothing | Android Y-plane brightness, contrast and horizontal sharpness available as `BarcodeCapture.imageQuality`, without a returned JPEG | Apple/web quality analysis is not implemented; measurements describe detected frames |
| Batch switch does nothing | `analyzeImages` with bounded concurrency, ordered output, empty detections and error propagation; flag selects sequential vs parallel default | Web file-path analysis is unsupported |
| Apple native build missing | Pinned ZXing 2.3.0 source, shared C ABI wrappers, CocoaPods source/header settings and SwiftPM native target now supplied | Xcode builds and iOS/macOS device tests; CI covers both dependency managers |
| Unsafe raw-buffer boundary | Dart validation, length-aware C ABI, native bounds checks, JNI buffer slices and CVPixelBuffer length | Further fuzzing and sanitizer coverage |
| Desktop capture missing | Linux/Windows native image decoding and native ABI CI | Live camera/preview backends remain unimplemented |
| Missing release/browser/desktop checks | Android release build in CI, headless Chrome reader regressions, Linux/Windows CTest jobs, real FFI fixture test | Physical camera E2E and executing remote CI |

## Validation commands

```sh
flutter analyze
flutter test
flutter test --platform chrome test/web/reader_browser_test.dart
cmake -S src -B build/native -DMS_ZXING_BUILD_TESTS=ON
cmake --build build/native --config Release --parallel 2
ctest --test-dir build/native -C Release --output-on-failure
MS_ZXING_NATIVE_TESTS=1 LD_LIBRARY_PATH="$PWD/build/native" flutter test test/ffi/zxing_native_test.dart test/ffi/postal_native_test.dart
cd example/android
./gradlew :mobile_scanner:testDebugUnitTest
cd ..
flutter build apk --release --target-platform android-arm64
```

The native Flutter fixture tests are opt-in so ordinary widget tests do not
require a prebuilt shared library. Native CI enables them explicitly on Linux.
Browser tests use mock detector results and real JavaScript interop; they do not
prove real-camera decoding. CTest reads a real QR fixture without an image library.
Apple's vendored source set was compiled and linked on Linux as an additional
check; this does not validate Swift imports, CocoaPods, SwiftPM or Apple runtime.

## Open feature work

DotCode and Linux/Windows live capture have not been marked complete. A DotCode
reader needs symbol sampling, error correction and reference images, or a decoder
library that actually supports it. ZXing 2.3.0 does not provide that decoder.
Desktop live capture needs an integrated frame/preview backend, lifecycle and
permission handling, camera selection, throttling and hardware validation on both
operating systems. File decoding does not supply those capabilities.

## Local verification

- 415 Flutter tests passed with the native fixture tests enabled.
- 59 Android JVM tests passed, including padded/strided frame quality buffers.
- Four headless Chrome reader regressions passed.
- Flutter analyzer: no issues.
- Native sized-ABI, crop and unsupported-format CTest passed.
- Apple vendored C++ sources compiled, linked and passed the same native fixture
  checks on Linux; Swift/Xcode validation remains pending.

Debug native builds also disable upstream image-dump diagnostics with NDEBUG,
preventing binarizer threshold images from being written to the app directory.

## rMQR follow-up

The pinned engine already decoded rectangular Micro QR symbols, but the bridge
mapped their format to unknown and could not request them explicitly. The C ABI
and Dart enum now expose `BarcodeFormat.rmQrCode` (67108864), and the WASM mapping
accepts canonical and legacy rMQR names. Native image tests cover small and large
real small, large, inverted and rotated rMQR fixtures, default/all detection
and isolation from standard QR and
Code 11 requests. Android live-camera validation still requires a device.

The rMQR follow-up passed 21 focused Flutter/FFI mapping tests and the native
C ABI fixture regression. The analyzer remained clean.

The release APK check also exposed library-level shrinking that removed the
Flutter plugin entry class before application registration. The library now
ships its classes to the consuming application's shrinker; consumer JNI rules
remain in effect. The example application's release still uses R8.

Android ARM64 release APK built successfully with application R8 enabled.
The release mapping retains the JNI class names, and the APK includes both
libms_zxing.so and libms_zxing_jni.so. Android live-camera testing still needs a
physical device. Build artifact: example/build/app/outputs/flutter-apk/app-release.apk.


## Complete format audit

See [FORMAT_SUPPORT_AUDIT.md](FORMAT_SUPPORT_AUDIT.md) for all 20 native formats,
all 41 WASM readable names, completed mapping/filter fixes, real fixtures and
engine-specific limits. This includes the 126/127 mask collision, DX film edge,
web Telepen/PZN/Code32, GS1/EAN supplements, metadata and Android Unicode fixes.
The native engine version remains 2.3.0; newer WASM-only readers are explicitly
identified rather than claimed on Android or Apple platforms.


Format-audit verification: 443 Flutter tests with native images, 59 Android JVM
tests, eight Chrome regressions and two native/JNI CTests passed. Static analysis
reported no issues. The full inventory and the narrower real-WASM image checks
are distinguished in the audit report.

The format-audit Android ARM64 release build also passed with application R8
active. The current APK includes the native format fixes and JNI metadata bridge.

## POSTNET and PLANET

Added both two-state postal image readers to the native bridge, including
explicit/all selection, required checksums, baseline-based direction, quarter-turn
rotation, optional inversion, crop-aware corners and decoded bytes. Public
selectors are `BarcodeFormat.postnet` and `BarcodeFormat.planet`. See the
[postal format audit](FORMAT_SUPPORT_AUDIT.md#postnet-and-planet-follow-up) for
fixture coverage and detector limits.

The native ABI now uses 64-bit masks/results and versioned v2 decode symbols.
Rebuild native libraries with the updated Dart/JNI/Swift bindings. Public IDs
for existing formats remain unchanged. Web postal decoding, four-state postal
formats, Apple builds and physical-camera validation remain outside this change.

Postal verification: 451 Flutter tests with native fixtures, 62 Android JVM
tests, eight Chrome regressions and two native/JNI CTests passed. Flutter
analysis reported no issues. Apple vendored C++ sources compiled and linked
on Linux; Swift/Xcode and camera-device validation remain pending.

The Android ARM64 release APK built successfully with application R8 enabled.
Both packaged native libraries reference the v2 decode ABI.
