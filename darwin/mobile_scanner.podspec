#
# To learn more about a Podspec see http://guides.cocoapods.org/syntax/podspec.html.
# Run `pod lib lint mobile_scanner.podspec` to validate before publishing.
#
Pod::Spec.new do |s|
  s.name             = 'mobile_scanner'
  s.version          = '8.0.0'
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

  # Both Apple build systems compile the same pinned native engine.
  s.source_files = [
    'mobile_scanner/Sources/mobile_scanner/**/*.swift',
    'mobile_scanner/Sources/MobileScannerNative/**/*.{h,c,cpp}',
  ]
  s.public_header_files = 'mobile_scanner/Sources/MobileScannerNative/include/ms_zxing.h'
  s.libraries = 'c++'
  s.compiler_flags = '-DZXING_READERS -DNDEBUG'
  s.pod_target_xcconfig.merge!({
    'CLANG_CXX_LANGUAGE_STANDARD' => 'c++20',
    'HEADER_SEARCH_PATHS' => '"$(PODS_TARGET_SRCROOT)/mobile_scanner/Sources/MobileScannerNative/zxing"',
    'SWIFT_ACTIVE_COMPILATION_CONDITIONS' => '$(inherited) MOBILE_SCANNER_ZXING',
  })
end
