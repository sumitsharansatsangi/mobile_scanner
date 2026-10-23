import com.android.Version
import org.jetbrains.kotlin.gradle.dsl.JvmTarget
import org.jetbrains.kotlin.gradle.dsl.KotlinAndroidProjectExtension

plugins {
    id("com.android.library")
}

group = "dev.steenbakker.mobile_scanner"
version = "1.0-SNAPSHOT"

// AGP 9+ compiles Kotlin itself when built-in Kotlin is enabled (the default),
// but apps migrated by the Flutter tool disable it via android.builtInKotlin=false.
// Apply KGP whenever built-in Kotlin is not active, so the Kotlin extension
// configured below exists in both configurations.
val agpMajor = Version.ANDROID_GRADLE_PLUGIN_VERSION.substringBefore('.').toInt()
val builtInKotlin = agpMajor >= 9 &&
    (findProperty("android.builtInKotlin")?.toString() ?: "true").toBoolean()
if (!builtInKotlin) {
    apply(plugin = "kotlin-android")
}


repositories {
    google()
    mavenCentral()
}

android {
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
                cppFlags("-std=c++20")
            }
        }
    }

    // Pin the NDK used to build the ZXing-C++ engine. 30.x is required for the
    // 16 KB page-size support flag set in the shared CMakeLists.
    ndkVersion = "30.0.15729638"

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
            isIncludeAndroidResources = true
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


// Configured through `extensions` rather than a `kotlin { }` block, because the
// Kotlin plugin is applied imperatively above and so has no type-safe accessor.
// The extension may not exist yet: with AGP 9 and android.builtInKotlin=false the
// Flutter Gradle Plugin applies KGP to this subproject after the script is evaluated.
fun configureKotlin() {
    extensions.configure(KotlinAndroidProjectExtension::class.java) {
        compilerOptions {
            jvmTarget = JvmTarget.JVM_21
        }
    }
}
if (extensions.findByName("kotlin") != null) {
    configureKotlin()
} else {
    plugins.withId("org.jetbrains.kotlin.android") { configureKotlin() }
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

    implementation("androidx.camera:camera-lifecycle:1.6.2")
    implementation("androidx.camera:camera-camera2:1.6.2")
    implementation("androidx.camera:camera-view:1.6.2")
    implementation("androidx.camera:camera-extensions:1.6.2")
    implementation("androidx.camera:camera-video:1.6.2")

    implementation("androidx.camera:camera-mlkit-vision:1.6.2")

    implementation("androidx.exifinterface:exifinterface:1.4.2")

    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.11.0")


    testImplementation(kotlin("test-junit"))
    testImplementation("org.mockito:mockito-core:5.23.0")
    testImplementation("org.robolectric:robolectric:4.16.1")
    testImplementation("junit:junit:4.13.2")
    testRuntimeOnly("org.junit.vintage:junit-vintage-engine:6.1.3")
    testRuntimeOnly("org.junit.platform:junit-platform-launcher:6.1.3")
    testImplementation("androidx.test:core:1.6.1")
    testImplementation("androidx.test.ext:junit:1.2.1")
}
