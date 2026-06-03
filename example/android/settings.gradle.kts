import java.util.Properties

pluginManagement {
    val flutterSdkPath = run {
        val properties = Properties()

        file("local.properties").inputStream().use {
            properties.load(it)
        }

        properties.getProperty("flutter.sdk")
            ?: error("flutter.sdk not set in local.properties")
    }

    includeBuild("$flutterSdkPath/packages/flutter_tools/gradle")

    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

plugins {
    id("dev.flutter.flutter-plugin-loader") version "1.0.0"

    // AGP 9+
    id("com.android.application") version "9.2.1" apply false
}

include(":app")