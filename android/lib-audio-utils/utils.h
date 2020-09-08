#ifndef UTILS_H_
#define UTILS_H_

#include <android/log.h>
#include <jni.h>
#include <stdbool.h>

#define TAG "xm_audio_utils_jni"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGF(...) __android_log_print(ANDROID_LOG_FATAL, TAG, __VA_ARGS__)

#define SUCCESS 0
#define INVALID_OBJECT -1
#define NO_FOUND_FIELD_OBJECT -2

#ifndef NELEM
#define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))
#endif

#endif  // UTILS_H_
