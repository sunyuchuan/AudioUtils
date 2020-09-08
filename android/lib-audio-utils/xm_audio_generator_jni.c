//
// Created by sunyc on 19-11-19.
//
#include "xm_audio_generator_jni.h"
#include "xm_android_jni.h"
#include "log.h"
#include "xm_audio_generator.h"
#include "utils.h"
#include <assert.h>
#include <pthread.h>

#define JNI_CLASS_AUDIO_GENERATOR "com/xmly/audio/utils/XmAudioGenerator"

extern void SetFFmpegLogLevel(int log_level);
extern bool J4A_ExceptionCheck__catchAll(JNIEnv *env);

typedef struct xm_audio_generator_fields_t {
    pthread_mutex_t mutex;
    jclass clazz;
    jfieldID field_mNativeXmAudioGenerator;
} xm_audio_generator_fields_t;

static xm_audio_generator_fields_t g_clazz;

static jlong
jni_mNativeXMAudioGenerator_get(JNIEnv *env, jobject thiz)
{
    return (*env)->GetLongField(env, thiz, g_clazz.field_mNativeXmAudioGenerator);
}

static void
jni_mNativeXMAudioGenerator_set(JNIEnv *env,
    jobject thiz, jlong value)
{
    (*env)->SetLongField(env, thiz, g_clazz.field_mNativeXmAudioGenerator, value);
}

static XmAudioGenerator *
jni_get_xm_audio_generator(JNIEnv *env, jobject thiz)
{
    pthread_mutex_lock(&g_clazz.mutex);

    XmAudioGenerator *ctx = (XmAudioGenerator *) (intptr_t) jni_mNativeXMAudioGenerator_get(env, thiz);
    if (ctx) {
        xmag_inc_ref(ctx);
    }

    pthread_mutex_unlock(&g_clazz.mutex);
    return ctx;
}

static XmAudioGenerator *
jni_set_xm_audio_generator(JNIEnv *env, jobject thiz,
    XmAudioGenerator *ctx)
{
    pthread_mutex_lock(&g_clazz.mutex);

    XmAudioGenerator *oldctx = (XmAudioGenerator *) (intptr_t) jni_mNativeXMAudioGenerator_get(env, thiz);
    if (ctx) {
        xmag_inc_ref(ctx);
    }
    jni_mNativeXMAudioGenerator_set(env, thiz, (intptr_t) ctx);

    pthread_mutex_unlock(&g_clazz.mutex);

    if (oldctx != NULL) {
        xmag_dec_ref_p(&oldctx);
    }

    return oldctx;
}

static void
XMAudioGenerator_release(JNIEnv *env, jobject thiz)
{
    LOGI("%s\n", __func__);
    XmAudioGenerator *ctx = jni_get_xm_audio_generator(env, thiz);
    if(ctx == NULL) {
        LOGW("XMAudioGenerator_release ctx is NULL\n");
        goto LABEL_RETURN;
    }

    jni_set_xm_audio_generator(env, thiz, NULL);
LABEL_RETURN:
    xmag_dec_ref_p(&ctx);
}

static void
XMAudioGenerator_stop(JNIEnv *env, jobject thiz)
{
    LOGI("%s\n", __func__);
    XmAudioGenerator *ctx = jni_get_xm_audio_generator(env, thiz);
    JNI_CHECK_GOTO(ctx, env, "java/lang/IllegalStateException", "AGjni: stop: null ctx", LABEL_RETURN);

    xm_audio_generator_stop(ctx);
LABEL_RETURN:
    xmag_dec_ref_p(&ctx);
}

static int
XMAudioGenerator_get_progress(JNIEnv *env, jobject thiz)
{
    int progress = 0;
    XmAudioGenerator *ctx = jni_get_xm_audio_generator(env, thiz);
    JNI_CHECK_GOTO(ctx, env, "java/lang/IllegalStateException", "AGjni: get_progress: null ctx", LABEL_RETURN);

    progress = xm_audio_generator_get_progress(ctx);
LABEL_RETURN:
    xmag_dec_ref_p(&ctx);
    return progress;
}

static int
XMAudioGenerator_start(JNIEnv *env, jobject thiz,
        jstring inConfigFilePath, jstring outM4aPath, jint encode_type)
{
    LOGI("%s\n", __func__);
    int ret = 0;
    XmAudioGenerator *ctx = jni_get_xm_audio_generator(env, thiz);
    JNI_CHECK_GOTO(ctx, env, "java/lang/IllegalStateException", "AGjni: start: null ctx", LABEL_RETURN);

    const char *in_config_path = NULL;
    const char *out_m4a_path = NULL;
    if (inConfigFilePath)
        in_config_path = (*env)->GetStringUTFChars(env, inConfigFilePath, 0);
    if (outM4aPath)
        out_m4a_path = (*env)->GetStringUTFChars(env, outM4aPath, 0);

    ret = xm_audio_generator_start(ctx, in_config_path, out_m4a_path, encode_type);
    if (in_config_path)
        (*env)->ReleaseStringUTFChars(env, inConfigFilePath, in_config_path);
    if (out_m4a_path)
        (*env)->ReleaseStringUTFChars(env, outM4aPath, out_m4a_path);
LABEL_RETURN:
    xmag_dec_ref_p(&ctx);
    return ret;
}

static void
XMAudioGenerator_close_log_file(JNIEnv *env, jobject thiz)
{
    LOGI("%s\n", __func__);
    AeCloseLogFile();
}

static void
XMAudioGenerator_set_log(JNIEnv *env, jobject thiz,
        jint logMode, jint logLevel,  jstring outLogPath)
{
    LOGI("%s\n", __func__);
    AeSetLogMode(logMode);
    AeSetLogLevel(logLevel);
    SetFFmpegLogLevel(logLevel);

    if(logMode == LOG_MODE_FILE) {
        if(outLogPath == NULL) {
            LOGE("logMode is LOG_MODE_FILE, and outLogPath is NULL, return\n");
            return;
        } else {
            const char *out_log_path_ = (*env)->GetStringUTFChars(env, outLogPath, 0);
            AeSetLogPath(out_log_path_);
            if (out_log_path_)
                (*env)->ReleaseStringUTFChars(env, outLogPath, out_log_path_);
        }
    }
}

static void
XMAudioGenerator_setup(JNIEnv *env, jobject thiz)
{
    LOGI("%s\n", __func__);
    XmAudioGenerator *ctx = xm_audio_generator_create();
    JNI_CHECK_GOTO(ctx, env, "java/lang/OutOfMemoryError", "AGjni: native_setup: create failed", LABEL_RETURN);

    jni_set_xm_audio_generator(env, thiz, ctx);
LABEL_RETURN:
    xmag_dec_ref_p(&ctx);
}

static JNINativeMethod g_methods[] = {
    { "native_setup", "()V", (void *) XMAudioGenerator_setup },
    { "native_set_log", "(IILjava/lang/String;)V", (void *) XMAudioGenerator_set_log },
    { "native_close_log_file", "()V", (void *) XMAudioGenerator_close_log_file },
    { "native_start", "(Ljava/lang/String;Ljava/lang/String;I)I", (void *) XMAudioGenerator_start },
    { "native_get_progress", "()I", (void *) XMAudioGenerator_get_progress },
    { "native_stop", "()V", (void *) XMAudioGenerator_stop },
    { "native_release", "()V", (void *) XMAudioGenerator_release },
};

int xm_audio_generator_global_init(JNIEnv *env) {
    int ret = 0;
    LOGD("xm_audio_generator_global_init");

    pthread_mutex_init(&g_clazz.mutex, NULL);

    IJK_FIND_JAVA_CLASS(env, g_clazz.clazz, JNI_CLASS_AUDIO_GENERATOR);
    (*env)->RegisterNatives(env, g_clazz.clazz, g_methods, NELEM(g_methods));

    g_clazz.field_mNativeXmAudioGenerator = (*env)->GetFieldID(env, g_clazz.clazz, "mNativeXmAudioGenerator", "J");
    return ret;
}

int xm_audio_generator_global_uninit() {
    int ret = 0;

    LOGD("xm_audio_generator_global_uninit");
    pthread_mutex_destroy(&g_clazz.mutex);
    return ret;
}

