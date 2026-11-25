# Windows build for mobile_scanner.
#
# Builds the shared ZXing-C++ engine (ms_zxing.dll) from the C ABI in ../src,
# plus the camera plugin (mobile_scanner_plugin.dll) that captures frames with
# Media Foundation and exposes them to Flutter as a texture.

cmake_minimum_required(VERSION 3.14)

set(PROJECT_NAME "mobile_scanner")
project(${PROJECT_NAME} LANGUAGES CXX)

# Build the shared engine from the cross-platform source.
get_filename_component(MS_SHARED_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../src" ABSOLUTE)
add_subdirectory("${MS_SHARED_SRC}" "${CMAKE_CURRENT_BINARY_DIR}/ms_zxing")

set(PLUGIN_NAME "mobile_scanner_plugin")

add_library(${PLUGIN_NAME} SHARED "mobile_scanner_plugin.cpp")
apply_standard_settings(${PLUGIN_NAME})
target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)
target_include_directories(${PLUGIN_NAME} INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(${PLUGIN_NAME} PRIVATE
  "${MS_SHARED_SRC}")
target_link_libraries(${PLUGIN_NAME} PRIVATE
  flutter
  flutter_wrapper_plugin
  ms_zxing
  mf
  mfplat
  mfreadwrite
  mfuuid
)

# Native libraries the Flutter app must bundle next to the .exe. Consumed by the
# Flutter Windows tooling via the `<plugin>_bundled_libraries` convention.
set(mobile_scanner_bundled_libraries
  "$<TARGET_FILE:ms_zxing>"
  "$<TARGET_FILE:${PLUGIN_NAME}>"
  PARENT_SCOPE
)
