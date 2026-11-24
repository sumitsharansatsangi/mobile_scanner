# Linux camera plugin and shared ZXing-C++ engine.

cmake_minimum_required(VERSION 3.18)

# The Flutter tooling sets PROJECT_NAME to "mobile_scanner" for this plugin.
set(PROJECT_NAME "mobile_scanner")
project(${PROJECT_NAME} LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# Build the shared engine from the cross-platform source.
get_filename_component(MS_SHARED_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../src" ABSOLUTE)
add_subdirectory("${MS_SHARED_SRC}" "${CMAKE_CURRENT_BINARY_DIR}/ms_zxing")
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0)
add_library(mobile_scanner_plugin SHARED mobile_scanner_plugin.cc)
apply_standard_settings(mobile_scanner_plugin)
target_link_libraries(mobile_scanner_plugin PRIVATE flutter PkgConfig::GTK PkgConfig::GSTREAMER ms_zxing)
target_include_directories(mobile_scanner_plugin PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
target_include_directories(mobile_scanner_plugin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# List of native libraries the Flutter app must bundle. Consumed by the Flutter
# Linux tooling via the `<plugin>_bundled_libraries` convention.
set(mobile_scanner_bundled_libraries
  "$<TARGET_FILE:ms_zxing>"
  "$<TARGET_FILE:mobile_scanner_plugin>"
  PARENT_SCOPE
)
