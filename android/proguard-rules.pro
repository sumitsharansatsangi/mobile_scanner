-keep class com.google.mlkit.** { *; }
-keep class com.google.android.gms.internal.mlkit_vision_barcode.* { *; }
-keep class com.google.android.libraries.barhopper.* { *; }
-keep class com.google.photos.* { *; }

-keepclassmembers class * extends java.lang.Enum {
    <fields>;
    public static **[] values();
    public static ** valueOf(java.lang.String);
}

# The custom ZXing JNI bridge looks up this class and constructor by name.
-keep class dev.steenbakker.mobile_scanner.engine.ZxingBarcode { *; }

# JNI exported function names include the engine class and native method names.
-keep class dev.steenbakker.mobile_scanner.engine.ZxingEngine {
    native <methods>;
}
