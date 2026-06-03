import com.android.build.api.dsl.LibraryExtension

plugins {
    id("com.android.library")
}

group = "dev.steenbakker.mobile_scanner"
version = "1.0-SNAPSHOT"

repositories {
    google()
    mavenCentral()
}

extensions.configure<LibraryExtension>("android") {
    namespace = "dev.steenbakker.mobile_scanner"

    compileSdk = 37

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_21
        targetCompatibility = JavaVersion.VERSION_21
    }

    sourceSets {
        getByName("main") {
            java.directories.add("src/main/kotlin")
        }

        getByName("test") {
            java.directories.add("src/test/kotlin")
        }
    }

    defaultConfig {
        minSdk = 24

        consumerProguardFiles("proguard-rules.pro")

        multiDexEnabled = true

        // Build the shared ZXing-C++ engine (libms_zxing.so) from the C ABI in
        // ../src via CMake. Loaded at runtime by Dart FFI and JNI.
        externalNativeBuild {
            cmake {
                cppFlags("-std=c++17")
            }
        }
    }

    // Pin the NDK used to build the ZXing-C++ engine. 30.x is required for the
    // 16 KB page-size support flag set in the shared CMakeLists.
    ndkVersion = "30.0.14904198"

    // Points at the Android CMake wrapper which pulls in ../../src/CMakeLists.txt.
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildTypes {
        getByName("debug") {
            isMinifyEnabled = false

            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }

        getByName("release") {
            isMinifyEnabled = true

            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }

    testOptions {
        unitTests {
            all {
                it.useJUnitPlatform()

                it.testLogging {
                    events(
                        "passed",
                        "skipped",
                        "failed",
                        "standardOut",
                        "standardError"
                    )

                    showStandardStreams = true
                }

                it.outputs.upToDateWhen { false }
            }
        }
    }
}


dependencies {
    val useUnbundled =
        project.findProperty("dev.steenbakker.mobile_scanner.useUnbundled")
            ?.toString()
            ?.toBoolean() ?: false

    if (useUnbundled) {
        implementation("com.google.android.gms:play-services-mlkit-barcode-scanning:18.3.1")
    } else {
        implementation("com.google.mlkit:barcode-scanning:17.3.0")
    }

    implementation("androidx.camera:camera-lifecycle:1.6.1")
    implementation("androidx.camera:camera-camera2:1.6.1")
    implementation("androidx.camera:camera-view:1.6.1")
    implementation("androidx.camera:camera-extensions:1.6.1")
    implementation("androidx.camera:camera-video:1.6.1")

    implementation("androidx.camera:camera-mlkit-vision:1.6.1")

    implementation("androidx.exifinterface:exifinterface:1.4.2")

    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.10.2")


    testImplementation(kotlin("test"))
    testImplementation("org.mockito:mockito-core:5.23.0")
    testImplementation("androidx.test:core:1.6.1")
    testImplementation("androidx.test.ext:junit:1.2.1")
}