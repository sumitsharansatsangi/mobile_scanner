# Linux FFI plugin build for mobile_scanner.
#
# Builds the shared ZXing-C++ engine (libms_zxing.so) from the shared C ABI in
# ../src and bundles it with the app. There is no platform-channel plugin on
# Linux — Dart talks to the engine directly through dart:ffi.

cmake_minimum_required(VERSION 3.18)

# The Flutter tooling sets PROJECT_NAME to "mobile_scanner" for this plugin.
set(PROJECT_NAME "mobile_scanner")
project(${PROJECT_NAME} LANGUAGES CXX)

# Build the shared engine from the cross-platform source.
get_filename_component(MS_SHARED_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../src" ABSOLUTE)
add_subdirectory("${MS_SHARED_SRC}" "${CMAKE_CURRENT_BINARY_DIR}/ms_zxing")

# List of native libraries the Flutter app must bundle. Consumed by the Flutter
# Linux tooling via the `<plugin>_bundled_libraries` convention.
set(mobile_scanner_bundled_libraries
  "$<TARGET_FILE:ms_zxing>"
  PARENT_SCOPE
)
