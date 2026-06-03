import java.util.Properties

plugins {
    id("com.android.application")
    id("dev.flutter.flutter-gradle-plugin")
}

val localProperties = Properties().apply {
    val file = rootProject.file("local.properties")
    if (file.exists()) {
        file.reader(Charsets.UTF_8).use(::load)
    }
}

val flutterVersionCode =
    localProperties.getProperty("flutter.versionCode")?.toInt() ?: 1

val flutterVersionName =
    localProperties.getProperty("flutter.versionName") ?: "1.0"

android {
    namespace = "dev.steenbakker.mobile_scanner_example"

    compileSdk = 37

    defaultConfig {
        applicationId = "dev.steenbakker.mobile_scanner_example"

        minSdk = 24
        targetSdk = 37

        versionCode = flutterVersionCode
        versionName = flutterVersionName

        ndkVersion = "30.0.14904198"
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_21
        targetCompatibility = JavaVersion.VERSION_21
    }

    buildTypes {
        release {
            signingConfig = signingConfigs.getByName("debug")
        }
    }
}

flutter {
    source = "../.."
}