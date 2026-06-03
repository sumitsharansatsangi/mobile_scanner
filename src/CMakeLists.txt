# Shared native build for the mobile_scanner ZXing-C++ core.
#
# This file is included by each platform's CMake project (Android, Linux,
# Windows) via add_subdirectory / include, and produces the `ms_zxing` library
# that exposes the C ABI in ms_zxing.h. zxing-cpp is fetched from source so we
# don't vendor a binary per platform.
#
# Platforms that don't use CMake (iOS/macOS) add zxing-cpp via SwiftPM/CocoaPods
# and compile ms_zxing.cpp directly in the darwin target instead.

cmake_minimum_required(VERSION 3.18)
project(ms_zxing LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)

# Pin a known-good zxing-cpp release. Bump deliberately and re-test all
# platforms; newer tags may change the ReaderOptions/Barcode API.
set(ZXING_CPP_TAG "v2.3.0" CACHE STRING "zxing-cpp git tag to build against")

# Build only the reader; skip writers, tests, examples to keep binaries small.
set(ZXING_READERS ON CACHE BOOL "" FORCE)
set(ZXING_WRITERS OFF CACHE BOOL "" FORCE)
set(ZXING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ZXING_BLACKBOX_TESTS OFF CACHE BOOL "" FORCE)
set(ZXING_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)  # static-link zxing into ms_zxing

FetchContent_Declare(
  zxing-cpp
  GIT_REPOSITORY https://github.com/zxing-cpp/zxing-cpp.git
  GIT_TAG ${ZXING_CPP_TAG}
  GIT_SHALLOW ON
)
FetchContent_MakeAvailable(zxing-cpp)

# The plugin's shared library that Dart FFI / JNI / Swift loads.
add_library(ms_zxing SHARED
  "${CMAKE_CURRENT_LIST_DIR}/ms_zxing.cpp"
)

target_include_directories(ms_zxing PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
target_include_directories(ms_zxing PRIVATE ${zxing-cpp_SOURCE_DIR}/core/src)
target_link_libraries(ms_zxing PRIVATE ZXing::ZXing)

include(CheckCXXSourceCompiles)
set(_old_CMAKE_REQUIRED_INCLUDES "${CMAKE_REQUIRED_INCLUDES}")
set(_old_CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}")
set(CMAKE_REQUIRED_INCLUDES "${zxing-cpp_SOURCE_DIR}/core/src")
set(CMAKE_REQUIRED_FLAGS "-std=c++17")
check_cxx_source_compiles(
  "#include <BarcodeFormat.h>\nint main() { auto f = ZXing::BarcodeFormat::DotCode; (void)f; return 0; }\n"
  ZXING_HAS_DOTCODE)
set(CMAKE_REQUIRED_INCLUDES "${_old_CMAKE_REQUIRED_INCLUDES}")
set(CMAKE_REQUIRED_FLAGS "${_old_CMAKE_REQUIRED_FLAGS}")

if(ZXING_HAS_DOTCODE)
  target_compile_definitions(ms_zxing PRIVATE ZXING_HAS_DOTCODE=1)
endif()

if(ANDROID)
  # Android 15+ ships 16 KB memory pages; shared libs must be aligned for them.
  # Requires NDK r27+ (we build with NDK 30.x).
  target_link_options(ms_zxing PRIVATE "-Wl,-z,max-page-size=16384")
  # zxing-cpp / our wrapper may emit __android_log_* when logging is enabled.
  find_library(log-lib log)
  target_link_libraries(ms_zxing PRIVATE ${log-lib})
  # Ensure proper C++ library linkage for NDK
  target_link_libraries(ms_zxing PRIVATE c++_static)
endif()

# DotCode support is detected from the actual zxing-cpp headers at configure time.

set_target_properties(ms_zxing PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
)
