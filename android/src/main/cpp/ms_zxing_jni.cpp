// JNI bridge between dev.steenbakker.mobile_scanner.engine.ZxingEngine (Kotlin)
// and the shared ms_zxing C ABI. Compiled into libms_zxing_jni.so, which links
// the cross-platform libms_zxing.so.

#include <jni.h>

#include <cstring>

#include "ms_zxing.h"

namespace {

// Cached references to the Kotlin ZxingBarcode class and its constructor.
jclass g_barcode_class = nullptr;
jmethodID g_barcode_ctor = nullptr;

bool ensureBarcodeClass(JNIEnv* env) {
  if (g_barcode_class != nullptr && g_barcode_ctor != nullptr) return true;

  jclass local = env->FindClass(
      "dev/steenbakker/mobile_scanner/engine/ZxingBarcode");
  if (local == nullptr) return false;

  // Keep a global ref so the class isn't unloaded between calls.
  g_barcode_class = static_cast<jclass>(env->NewGlobalRef(local));
  env->DeleteLocalRef(local);
  if (g_barcode_class == nullptr) return false;

  // ZxingBarcode(long format, String text, byte[] rawBytes, float[] corners)
  g_barcode_ctor = env->GetMethodID(
      g_barcode_class, "<init>", "(JLjava/lang/String;[B[F)V");
  return g_barcode_ctor != nullptr;
}

// NewStringUTF expects modified UTF-8 and corrupts four-byte Unicode (emoji).
// The String byte-array constructor decodes standard UTF-8 from ZXing correctly.
jstring fromUtf8(JNIEnv* env, const char* text) {
  if (text == nullptr) return nullptr;
  const auto length = static_cast<jsize>(std::strlen(text));
  jbyteArray bytes = env->NewByteArray(length);
  if (bytes == nullptr) return nullptr;
  env->SetByteArrayRegion(bytes, 0, length, reinterpret_cast<const jbyte*>(text));
  jclass stringClass = env->FindClass("java/lang/String");
  jmethodID constructor = env->GetMethodID(stringClass, "<init>", "([BLjava/lang/String;)V");
  jstring encoding = env->NewStringUTF("UTF-8");
  auto result = static_cast<jstring>(env->NewObject(stringClass, constructor, bytes, encoding));
  env->DeleteLocalRef(encoding);
  env->DeleteLocalRef(stringClass);
  env->DeleteLocalRef(bytes);
  return result;
}

jobject toJavaMetadata(JNIEnv* env, const MsZxingResultMetadata* metadata) {
  if (metadata == nullptr) return nullptr;
  jclass mapClass = env->FindClass("java/util/HashMap");
  jobject map = env->NewObject(mapClass, env->GetMethodID(mapClass, "<init>", "()V"));
  jmethodID put = env->GetMethodID(mapClass, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
  auto set = [&](const char* name, jobject value) {
    jstring key = env->NewStringUTF(name);
    jobject previous = env->CallObjectMethod(map, put, key, value);
    if (previous != nullptr) env->DeleteLocalRef(previous);
    env->DeleteLocalRef(key);
    if (value != nullptr) env->DeleteLocalRef(value);
  };
  set("formatName", fromUtf8(env, metadata->format_name));
  set("symbologyIdentifier", fromUtf8(env, metadata->symbology_identifier));
  set("version", fromUtf8(env, metadata->version));
  set("errorCorrectionLevel", fromUtf8(env, metadata->error_correction_level));
  set("sequenceId", fromUtf8(env, metadata->sequence_id));
  jclass integer = env->FindClass("java/lang/Integer");
  jmethodID integerValue = env->GetStaticMethodID(integer, "valueOf", "(I)Ljava/lang/Integer;");
  set("sequenceSize", env->CallStaticObjectMethod(integer, integerValue, metadata->sequence_size));
  set("sequenceIndex", env->CallStaticObjectMethod(integer, integerValue, metadata->sequence_index));
  jclass boolean = env->FindClass("java/lang/Boolean");
  jmethodID booleanValue = env->GetStaticMethodID(boolean, "valueOf", "(Z)Ljava/lang/Boolean;");
  set("isInverted", env->CallStaticObjectMethod(boolean, booleanValue, metadata->is_inverted != 0 ? JNI_TRUE : JNI_FALSE));
  set("isMirrored", env->CallStaticObjectMethod(boolean, booleanValue, metadata->is_mirrored != 0 ? JNI_TRUE : JNI_FALSE));
  env->DeleteLocalRef(boolean);
  env->DeleteLocalRef(integer);
  env->DeleteLocalRef(mapClass);
  return map;
}

jobject toJavaBarcode(JNIEnv* env, const MsZxingResult& r, const MsZxingResultMetadata* metadata) {
  jstring text = nullptr;
  if (r.text != nullptr) {
    text = fromUtf8(env, r.text);
  }

  jbyteArray bytes = nullptr;
  if (r.bytes != nullptr && r.bytes_length > 0) {
    bytes = env->NewByteArray(r.bytes_length);
    if (bytes != nullptr) {
      env->SetByteArrayRegion(
          bytes, 0, r.bytes_length,
          reinterpret_cast<const jbyte*>(r.bytes));
    }
  }

  jfloat cornerValues[8] = {
      r.corners[0].x, r.corners[0].y, r.corners[1].x, r.corners[1].y,
      r.corners[2].x, r.corners[2].y, r.corners[3].x, r.corners[3].y,
  };
  jfloatArray corners = env->NewFloatArray(8);
  if (corners != nullptr) {
    env->SetFloatArrayRegion(corners, 0, 8, cornerValues);
  }

  jobject obj = env->NewObject(g_barcode_class, g_barcode_ctor, r.format, text,
                               bytes, corners);

  if (obj != nullptr && metadata != nullptr) {
    jobject details = toJavaMetadata(env, metadata);
    jfieldID field = env->GetFieldID(g_barcode_class, "metadata", "Ljava/util/Map;");
    env->SetObjectField(obj, field, details);
    if (details != nullptr) env->DeleteLocalRef(details);
  }
  if (text != nullptr) env->DeleteLocalRef(text);
  if (bytes != nullptr) env->DeleteLocalRef(bytes);
  if (corners != nullptr) env->DeleteLocalRef(corners);
  return obj;
}

}  // namespace

extern "C" JNIEXPORT jobjectArray JNICALL
Java_dev_steenbakker_mobile_1scanner_engine_ZxingEngine_nativeDecodeLuma(
    JNIEnv* env, jobject /*thiz*/, jobject luma, jint width, jint height,
    jint rowStride, jlong formatMask, jint cropLeft, jint cropTop,
    jint cropWidth, jint cropHeight, jboolean tryHarder, jboolean tryRotate,
    jboolean tryInvert, jboolean tryDownscale, jint maxSymbols) {
  if (!ensureBarcodeClass(env)) return nullptr;

  auto* data = static_cast<const uint8_t*>(env->GetDirectBufferAddress(luma));
  if (data == nullptr) return nullptr;

  MsZxingDecodeParams params;
  std::memset(&params, 0, sizeof(params));
  params.data = data;
  params.width = width;
  params.height = height;
  params.row_stride = rowStride;
  params.pixel_format = MS_ZXING_PIXEL_LUM;
  params.format_mask = static_cast<uint64_t>(formatMask);
  params.crop_left = cropLeft;
  params.crop_top = cropTop;
  params.crop_width = cropWidth;
  params.crop_height = cropHeight;
  params.try_harder = tryHarder ? 1 : 0;
  params.try_rotate = tryRotate ? 1 : 0;
  params.try_invert = tryInvert ? 1 : 0;
  params.try_downscale = tryDownscale ? 1 : 0;
  params.max_symbols = maxSymbols;

  MsZxingResultList* list = ms_zxing_decode_sized_v2(&params, static_cast<uint64_t>(env->GetDirectBufferCapacity(luma)));
  if (list == nullptr) return nullptr;

  jobjectArray array =
      env->NewObjectArray(list->count, g_barcode_class, nullptr);
  if (array != nullptr) {
    for (int32_t i = 0; i < list->count; ++i) {
      jobject barcode = toJavaBarcode(env, list->results[i], ms_zxing_result_metadata(list, i));
      if (barcode != nullptr) {
        env->SetObjectArrayElement(array, i, barcode);
        env->DeleteLocalRef(barcode);
      }
    }
  }

  ms_zxing_free_results(list);
  return array;
}

extern "C" JNIEXPORT jstring JNICALL
Java_dev_steenbakker_mobile_1scanner_engine_ZxingEngine_nativeVersion(
    JNIEnv* env, jobject /*thiz*/) {
  return env->NewStringUTF(ms_zxing_version());
}
