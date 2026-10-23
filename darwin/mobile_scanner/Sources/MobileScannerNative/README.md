ZXing-C++ core sources from release v2.3.0, unmodified except the exclusions
used by upstream SwiftPM (ZXingC.cpp, ZXingCpp.cpp, libzint).
Source: https://github.com/zxing-cpp/zxing-cpp/tree/v2.3.0/core/src
Licenses: Apache-2.0 and BSD-3-Clause (libzueci), reproduced in ZXING_LICENSE;
individual notices retained.

Version.h is generated from upstream core/Version.h.in for reader-only 2.3.0.

The small wrapper translation units include the shared repository src/ engine.
The public C header is a copy of src/ms_zxing.h; keep both identical. Vendoring
allows CocoaPods and SwiftPM to use the same pinned engine without an optional
manual build step. Update this copy together with ZXING_CPP_TAG in src/CMakeLists.txt.

Both Apple build systems define NDEBUG for the native target, including debug
apps. This disables upstream image-dump diagnostics in the binarizer.
