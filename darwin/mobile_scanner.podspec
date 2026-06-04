#
# To learn more about a Podspec see http://guides.cocoapods.org/syntax/podspec.html.
# Run `pod lib lint mobile_scanner.podspec` to validate before publishing.
#
Pod::Spec.new do |s|
  s.name             = 'mobile_scanner'
  s.version          = '7.0.0'
  s.summary          = 'An universal scanner for Flutter based on the Vision API.'
  s.description      = <<-DESC
An universal scanner for Flutter based on the Vision API.
                       DESC
  s.homepage         = 'https://github.com/juliansteenbakker/mobile_scanner'
  s.license          = { :file => '../LICENSE' }
  s.author           = { 'Julian Steenbakker' => 'juliansteenbakker@outlook.com' }
  s.source           = { :path => '.' }
  s.source_files = 'mobile_scanner/Sources/mobile_scanner/**/*.swift'
  s.ios.dependency 'Flutter'
  s.osx.dependency 'FlutterMacOS'
  s.ios.deployment_target = '12.0'
  s.osx.deployment_target = '10.14'
  # Flutter.framework does not contain a i386 slice.
  s.pod_target_xcconfig = { 'DEFINES_MODULE' => 'YES', 'EXCLUDED_ARCHS[sdk=iphonesimulator*]' => 'i386' }
  s.swift_version = '5.0'
  s.resource_bundles = {'mobile_scanner_privacy' => ['mobile_scanner/Sources/mobile_scanner/Resources/PrivacyInfo.xcprivacy']}

  # ---------------------------------------------------------------------------
  # Optional: ZXing-C++ primary engine (DataBar / MaxiCode / DotCode / Code 11 /
  # MSI / Pharmacode + faster decoding). Disabled by default so the plugin
  # builds with Apple Vision only.
  #
  # To enable on iOS/macOS:
  #   1. Vendor zxing-cpp:
  #        git submodule add https://github.com/zxing-cpp/zxing-cpp \
  #          darwin/third_party/zxing-cpp   # checkout tag v2.3.0
  #   2. Compile the shared C ABI + zxing-cpp core and expose the C header so
  #      Swift can call it, then define the MOBILE_SCANNER_ZXING flag:
  #
  #        s.source_files = [
  #          'mobile_scanner/Sources/mobile_scanner/**/*.swift',
  #          '../src/ms_zxing.{h,cpp}',
  #          '../src/ms_fallback_barcodes.{h,cpp}',
  #          'third_party/zxing-cpp/core/src/**/*.{h,c,cc,cpp}',
  #        ]
  #        s.public_header_files = '../src/ms_zxing.h'
  #        s.libraries = 'c++'
  #        s.pod_target_xcconfig = {
  #          'DEFINES_MODULE' => 'YES',
  #          'EXCLUDED_ARCHS[sdk=iphonesimulator*]' => 'i386',
  #          'CLANG_CXX_LANGUAGE_STANDARD' => 'c++20',
  #          'HEADER_SEARCH_PATHS' => '"$(PODS_TARGET_SRCROOT)/third_party/zxing-cpp/core/src" "$(PODS_TARGET_SRCROOT)/../src"',
  #          'SWIFT_ACTIVE_COMPILATION_CONDITIONS' => 'MOBILE_SCANNER_ZXING',
  #          'GCC_PREPROCESSOR_DEFINITIONS' => 'ZXING_HAS_DOTCODE=1',
  #        }
  #
  #   3. `cd example/ios && pod install` (and example/macos) then rebuild.
  #
  # NOTE: this wiring needs validation in Xcode; the exact zxing-cpp source glob
  # may vary by release. See ZxingScannerDarwin.swift for the Swift side.
  # ---------------------------------------------------------------------------
end
