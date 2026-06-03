# Android CMake wrapper for the mobile_scanner native ZXing-C++ engine.
#
# All the real work (fetching zxing-cpp + building the ms_zxing C ABI library)
# lives in the shared ../../../../src/CMakeLists.txt so Android, Linux and
# Windows share one definition. This file just includes it.

cmake_minimum_required(VERSION 3.22.1)
project(mobile_scanner_native LANGUAGES CXX)

# Resolve the plugin's shared native source directory (plugin_root/src).
get_filename_component(MS_SHARED_SRC
  "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src" ABSOLUTE)

# Builds libms_zxing.so (the C ABI core) from the cross-platform source.
add_subdirectory("${MS_SHARED_SRC}" "${CMAKE_CURRENT_BINARY_DIR}/ms_zxing")

# JNI bridge consumed by ZxingEngine.kt. Links the C ABI core above.
add_library(ms_zxing_jni SHARED ms_zxing_jni.cpp)
target_link_libraries(ms_zxing_jni PRIVATE ms_zxing)
