// swift-tools-version: 5.9
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "mobile_scanner",
    platforms: [
        .iOS("12.0"),
        .macOS("10.14")
    ],
    products: [
        .library(name: "mobile-scanner", targets: ["mobile_scanner"])
    ],
    dependencies: [],
    targets: [
        .target(
            name: "mobile_scanner",
            dependencies: ["MobileScannerNative"],
            resources: [
                .process("Resources"),
            ],
            swiftSettings: [.define("MOBILE_SCANNER_ZXING")]
        ),
        .target(
            name: "MobileScannerNative",
            exclude: ["README.md", "ZXING_LICENSE"],
            publicHeadersPath: "include",
            cSettings: [.define("ZXING_READERS")],
            cxxSettings: [.define("ZXING_READERS"), .headerSearchPath("zxing")],
            linkerSettings: [.linkedLibrary("c++")]
        )
    ],
    cxxLanguageStandard: .cxx20
)
