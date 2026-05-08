import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
    id("com.android.library")
    id("org.jetbrains.kotlin.android") version "2.3.21"
    id("org.jetbrains.kotlin.kapt")
    id("com.google.dagger.hilt.android") version "2.50"
}

group = "dev.steenbakker.mobile_scanner"
version = "1.0-SNAPSHOT"

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
            java.srcDirs("src/main/kotlin")
        }

        getByName("test") {
            java.srcDirs("src/test/kotlin")
        }
    }

    defaultConfig {
        minSdk = 24

        consumerProguardFiles("proguard-rules.pro")

        multiDexEnabled = true
    }

    buildTypes {
        getByName("debug") {
            isMinifyEnabled = false

            proguardFiles(
                getDefaultProguardFile("proguard-android.txt"),
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

kotlin {
    compilerOptions {
        jvmTarget.set(JvmTarget.JVM_21)

        freeCompilerArgs.addAll(
            listOf(
                "-opt-in=kotlin.RequiresOptIn",
                "-opt-in=kotlinx.coroutines.ExperimentalCoroutinesApi",
                "-opt-in=androidx.camera.core.ExperimentalGetImage"
            )
        )
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

    implementation("androidx.exifinterface:exifinterface:1.4.0-alpha01")

    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.10.2")

    implementation("com.google.dagger:hilt-android:2.50")
    kapt("com.google.dagger:hilt-compiler:2.50")

    testImplementation(kotlin("test"))
    testImplementation("org.mockito:mockito-core:5.21.0")
    testImplementation("androidx.test:core:1.6.1")
    testImplementation("androidx.test.ext:junit:1.2.1")
}