//
// Created by sunyc on 19-10-10.
//
#include "xm_android_jni.h"
#include "log.h"
#include "xm_audio_utils.h"
#include "xm_duration_parser.h"
#include "utils.h"
#include <assert.h>
#include <pthread.h>
#include "xm_audio_generator_jni.h"

#define JNI_CLASS_AUDIO_UTILS "com/xmly/audio/utils/XmAudioUtils"

extern void SetFFmpegLogLevel(int log_level);
extern void RegisterFFmpeg();
extern bool J4A_ExceptionCheck__catchAll(JNIEnv *env);

typedef struct xm_audio_utils_fields_t {
    pthread_mutex_t mutex;
    jclass clazz;
    jfieldID field_mNativeXmAudioUtils;
} xm_audio_utils_fields_t;

static xm_audio_utils_fields_t g_clazz;
static JavaVM* g_jvm;

jlong jni_mNativeXMAudioUtils_get(JNIEnv *env, jobject thiz)
{
    return (*env)->GetLongField(env, thiz, g_clazz.field_mNativeXmAudioUtils);
}

static void jni_mNativeXMAudioUtils_set(JNIEnv *env, jobject thiz, jlong value)
{
    (*env)->SetLongField(env, thiz, g_clazz.field_mNativeXmAudioUtils, value);
}

static XmAudioUtils *jni_get_xm_audio_utils(JNIEnv *env, jobject thiz)
{
    pthread_mutex_lock(&g_clazz.mutex);

    XmAudioUtils *ctx = (XmAudioUtils *) (intptr_t) jni_mNativeXMAudioUtils_get(env, thiz);
    if (ctx) {
        xmau_inc_ref(ctx);
    }

    pthread_mutex_unlock(&g_clazz.mutex);
    return ctx;
}

static XmAudioUtils *jni_set_xm_audio_utils(JNIEnv *env, jobject thiz, XmAudioUtils *ctx)
{
    pthread_mutex_lock(&g_clazz.mutex);

    XmAudioUtils *oldctx = (XmAudioUtils *) (intptr_t) jni_mNativeXMAudioUtils_get(env, thiz);
    if (ctx) {
        xmau_inc_ref(ctx);
    }
    jni_mNativeXMAudioUtils_set(env, thiz, (intptr_t) ctx);

    pthread_mutex_unlock(&g_clazz.mutex);

    if (oldctx != NULL) {
        xmau_dec_ref_p(&oldctx);
    }

    return oldctx;
}

static void
XMAudioUtils_release(JNIEnv *env, jobject thiz)
{
    LOGI("%s\n", __func__);
    XmAudioUtils *ctx = jni_get_xm_audio_utils(env, thiz);
    if(ctx == NULL) {
        LOGW("XMAudioUtils_release ctx is NULL\n");
        goto LABEL_RETURN;
    }

    jni_set_xm_audio_utils(env, thiz, NULL);
LABEL_RETURN:
    xmau_dec_ref_p(&ctx);
}

static int
XMAudioUtils_get_effects_frame(JNIEnv *env, jobject thiz,
    jshortArray buffer, jint buffer_size_in_short, jint action_type)
{
    int ret = 0;
    XmAudioUtils *ctx = jni_get_xm_audio_utils(env, thiz);
    JNI_CHECK_GOTO(ctx, env, "java/lang/IllegalStateException", "AUjni: mixer get frame: null ctx", LABEL_RETURN);

    jshort *buffer_ = (*env)->GetShortArrayElements(env, buffer, NULL);
    switch(action_type) {
        case MIXER_MIX:
            ret = xm_audio_utils_mixer_get_frame(ctx, buffer_,
                buffer_size_in_short);
            break;
        default:
            ret = -1;
            LOGE("%s invalid action_type %d.\n", __func__, action_type);
            break;
    }
    (*env)->ReleaseShortArrayElements(env, buffer, buffer_, 0);
LABEL_RETURN:
    xmau_dec_ref_p(&ctx);
    return ret;
}

static int
XMAudioUtils_effects_seekTo(JNIEnv *env, jobject thiz,
    jint seekTimeMs, jint action_type)
{
    LOGI("%s\n", __func__);
    int ret = 0;
    XmAudioUtils *ctx = jni_get_xm_audio_utils(env, thiz);
    JNI_CHECK_GOTO(ctx, env, "java/lang/IllegalStateException", "AUjni: mixer seekTo: null ctx", LABEL_RETURN);

    switch(action_type) {
        case MIXER_MIX:
            ret = xm_audio_utils_mixer_seekTo(ctx, seekTimeMs);
            break;
        default:
            ret = -1;
            LOGE("%s invalid action_type %d.\n", __func__, action_type);
            break;
    }
LABEL_RETURN:
    xmau_dec_ref_p(&ctx);
    return ret;
}

static int
XMAudioUtils_effects_init(JNIEnv *env, jobject thiz,
    jstring inConfigFilePath, jint action_type)
{
    LOGI("%s\n", __func__);
    int ret = 0;
    XmAudioUtils *ctx = jni_get_xm_audio_utils(env, thiz);
    JNI_CHECK_GOTO(ctx, env, "java/lang/IllegalStateException", "AUjni: mixer init: null ctx", LABEL_RETURN);

    const char *in_config_path = NULL;
    if (inConfigFilePath)
        in_config_path = (*env)->GetStringUTFChars(env, inConfigFilePath, 0);

    switch(action_type) {
        case MIXER_MIX:
            ret = xm_audio_utils_mixer_init(ctx, in_config_path);
            break;
        default:
            ret = -1;
            LOGE("%s invalid action_type %d.\n", __func__, action_type);
            break;
    }

    if (in_config_path)
        (*env)->ReleaseStringUTFChars(env, inConfigFilePath, in_config_path);
LABEL_RETURN:
    xmau_dec_ref_p(&ctx);
    return ret;
}

static int
XMAudioUtils_fade(JNIEnv *env, jobject thiz,
    jshortArray buffer, jint buffer_size_in_short, jint buffer_start_time_ms)
{
    int ret = -1;
    XmAudioUtils *ctx = jni_get_xm_audio_utils(env, thiz);
    JNI_CHECK_GOTO(ctx, env, "java/lang/IllegalStateException", "AUjni: fade: null ctx", LABEL_RETURN);

    jshort *buffer_ = (*env)->GetShortArrayElements(env, buffer, NULL);
    ret = xm_audio_utils_fade(ctx, buffer_, buffer_size_in_short, buffer_start_time_ms);
    (*env)->ReleaseShortArrayElements(env, buffer, buffer_, 0);
LABEL_RETURN:
    xmau_dec_ref_p(&ctx);
    return ret;
}

static int
XMAudioUtils_fade_init(JNIEnv *env, jobject thiz,
    jint pcmSampleRate, jint pcmNbChannels, jint audioStartTimeMs,
    jint audioEndTimeMs, jint fadeInTimeMs, jint fadeOutTimeMs)
{
    LOGI("%s\n", __func__);
    int ret = -1;
    XmAudioUtils *ctx = jni_get_xm_audio_utils(env, thiz);
    JNI_CHECK_GOTO(ctx, env, "java/lang/IllegalStateException", "AUjni: fade init: null ctx", LABEL_RETURN);

    ret = xm_audio_utils_fade_init(ctx, pcmSampleRate, pcmNbChannels,
        audioStartTimeMs, audioEndTimeMs, fadeInTimeMs, fadeOutTimeMs);

LABEL_RETURN:
    xmau_dec_ref_p(&ctx);
    return ret;
}

static int
XMAudioUtils_get_decoded_frame(JNIEnv *env, jobject thiz,
    jshortArray buffer, jint buffer_size_in_short, jboolean loop)
{
    int ret = -1;
    XmAudioUtils *ctx = jni_get_xm_audio_utils(env, thiz);
    JNI_CHECK_GOTO(ctx, env, "java/lang/IllegalStateException", "AUjni: get_decoded_frame: null ctx", LABEL_RETURN);

    jshort *buffer_ = (*env)->GetShortArrayElements(env, buffer, NULL);
    ret = xm_audio_utils_get_decoded_frame(ctx, buffer_,
        buffer_size_in_short, loop);
    (*env)->ReleaseShortArrayElements(env, buffer, buffer_, 0);

LABEL_RETURN:
    xmau_dec_ref_p(&ctx);
    return ret;
}

static void
XMAudioUtils_decoder_seekTo(JNIEnv *env, jobject thiz,
    jint seekTimeMs)
{
    LOGI("%s\n", __func__);
    XmAudioUtils *ctx = jni_get_xm_audio_utils(env, thiz);
    JNI_CHECK_GOTO(ctx, env, "java/lang/IllegalStateException", "AUjni: decoder_seekTo: null ctx", LABEL_RETURN);

    xm_audio_utils_decoder_seekTo(ctx, seekTimeMs);
LABEL_RETURN:
    xmau_dec_ref_p(&ctx);
}

static int
XMAudioUtils_decoder_create(JNIEnv *env, jobject thiz,
    jstring inAudioPath, jint cropStartTimeInMs, jint cropEndTimeInMs,
    jint outSampleRate, jint outChannels, jint volume)
{
    LOGI("%s\n", __func__);
    int ret = -1;
    XmAudioUtils *ctx = jni_get_xm_audio_utils(env, thiz);
    JNI_CHECK_GOTO(ctx, env, "java/lang/IllegalStateException", "AUjni: decoder_create: null ctx", LABEL_RETURN);

    const char *in_audio_path = NULL;
    if (inAudioPath)
        in_audio_path = (*env)->GetStringUTFChars(env, inAudioPath, 0);

    ret = xm_audio_utils_decoder_create(ctx, in_audio_path,
        cropStartTimeInMs, cropEndTimeInMs,
        outSampleRate, outChannels, volume);

    if (in_audio_path)
        (*env)->ReleaseStringUTFChars(env, inAudioPath, in_audio_path);
LABEL_RETURN:
    xmau_dec_ref_p(&ctx);
    return ret;
}

static int
XMAudioUtils_resampler_resample(JNIEnv *env, jobject thiz,
    jshortArray buffer, jint buffer_size_in_short)
{
    int ret = -1;
    XmAudioUtils *ctx = jni_get_xm_audio_utils(env, thiz);
    JNI_CHECK_GOTO(ctx, env, "java/lang/IllegalStateException", "AUjni: resampler_resample: null ctx", LABEL_RETURN);

    jshort *buffer_ = (*env)->GetShortArrayElements(env, buffer, NULL);
    ret = xm_audio_utils_pcm_resampler_resample(ctx, buffer_,
        buffer_size_in_short);
    (*env)->ReleaseShortArrayElements(env, buffer, buffer_, 0);

LABEL_RETURN:
    xmau_dec_ref_p(&ctx);
    return ret;
}

static bool
XMAudioUtils_resampler_init(JNIEnv *env, jobject thiz,
    jstring inAudioPath, jboolean isPcm, jint srcSampleRate, jint srcChannels,
    jdouble dstSampleRate, jint dstChannels)
{
    LOGI("%s\n", __func__);
    int ret = -1;
    XmAudioUtils *ctx = jni_get_xm_audio_utils(env, thiz);
    JNI_CHECK_GOTO(ctx, env, "java/lang/IllegalStateException", "AUjni: resampler_init: null ctx", LABEL_RETURN);

    const char *in_audio_path = NULL;
    if (inAudioPath)
        in_audio_path = (*env)->GetStringUTFChars(env, inAudioPath, 0);

    ret = xm_audio_utils_pcm_resampler_init(ctx, in_audio_path, isPcm,
        srcSampleRate, srcChannels, dstSampleRate, dstChannels);

    if (in_audio_path)
        (*env)->ReleaseStringUTFChars(env, inAudioPath, in_audio_path);
LABEL_RETURN:
    xmau_dec_ref_p(&ctx);
    return ret;
}

static void
XMAudioUtils_close_log_file(JNIEnv *env, jobject thiz)
{
    LOGI("%s\n", __func__);
    AeCloseLogFile();
}

static void
XMAudioUtils_set_log(JNIEnv *env, jobject thiz,
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
XMAudioUtils_setup(JNIEnv *env, jobject thiz)
{
    LOGI("%s\n", __func__);
    XmAudioUtils *ctx = xm_audio_utils_create();
    JNI_CHECK_GOTO(ctx, env, "java/lang/OutOfMemoryError", "AUjni: native_setup: create failed", LABEL_RETURN);

    jni_set_xm_audio_utils(env, thiz, ctx);
LABEL_RETURN:
    xmau_dec_ref_p(&ctx);
}

static int
XMAudioUtils_GetAudioFileDuration(JNIEnv *env,
    jobject thiz, jstring inAudioPath, jboolean isPcm, jint bitsPerSample,
    jint srcSampleRate, jint srcChannels) {
    int ret = 0;
    const char *in_audio_path = NULL;
    if (inAudioPath)
        in_audio_path = (*env)->GetStringUTFChars(env, inAudioPath, 0);

    ret = get_file_duration_ms(in_audio_path, isPcm,
        bitsPerSample, srcSampleRate, srcChannels);

    if (in_audio_path)
        (*env)->ReleaseStringUTFChars(env, inAudioPath, in_audio_path);
    return ret;
}

static int
XMAudioUtils_StereoToMonoS16(JNIEnv *env, jobject thiz,
    jshortArray dstBuffer, jshortArray srcBuffer, jint nbSamples, jint indexChannels) {
    int ret = 0;
    jshort *dst_buffer_ = (*env)->GetShortArrayElements(env, dstBuffer, NULL);
    jshort *src_buffer_ = (*env)->GetShortArrayElements(env, srcBuffer, NULL);
    if(dst_buffer_ == NULL || src_buffer_ == NULL) {
        ret = 0;
        goto fail;
    }

    short *p = src_buffer_;
    short *q = dst_buffer_;
    int n = nbSamples;
    int index = indexChannels;
    while (n >= 4) {
        q[0] = p[0 + index];
        q[1] = p[2 + index];
        q[2] = p[4 + index];
        q[3] = p[6 + index];
        q += 4;
        p += 8;
        n -= 4;
    }
    while (n > 0) {
        q[0] = p[0 + index];
        q++;
        p += 2;
        n--;
    }
    ret = nbSamples;

fail:
    if (dst_buffer_) (*env)->ReleaseShortArrayElements(env, dstBuffer, dst_buffer_, 0);
    if (src_buffer_) (*env)->ReleaseShortArrayElements(env, srcBuffer, src_buffer_, 0);
    return ret;
}

static JNINativeMethod g_methods[] = {
    { "native_StereoToMonoS16", "([S[SII)I", (void *) XMAudioUtils_StereoToMonoS16 },
    { "native_GetAudioFileDuration", "(Ljava/lang/String;ZIII)I", (void *) XMAudioUtils_GetAudioFileDuration },
    { "native_setup", "()V", (void *) XMAudioUtils_setup },
    { "native_resampler_init", "(Ljava/lang/String;ZIIDI)Z", (void *) XMAudioUtils_resampler_init },
    { "native_resampler_resample", "([SI)I", (void *) XMAudioUtils_resampler_resample },
    { "native_set_log", "(IILjava/lang/String;)V", (void *) XMAudioUtils_set_log },
    { "native_close_log_file", "()V", (void *) XMAudioUtils_close_log_file },
    { "native_decoder_create", "(Ljava/lang/String;IIIII)I", (void *) XMAudioUtils_decoder_create },
    { "native_decoder_seekTo", "(I)V", (void *) XMAudioUtils_decoder_seekTo },
    { "native_get_decoded_frame", "([SIZ)I", (void *) XMAudioUtils_get_decoded_frame },
    { "native_fade_init", "(IIIIII)I", (void *) XMAudioUtils_fade_init },
    { "native_fade", "([SII)I", (void *) XMAudioUtils_fade },
    { "native_effects_init", "(Ljava/lang/String;I)I", (void *) XMAudioUtils_effects_init },
    { "native_effects_seekTo", "(II)I", (void *) XMAudioUtils_effects_seekTo },
    { "native_get_effects_frame", "([SII)I", (void *) XMAudioUtils_get_effects_frame },
    { "native_release", "()V", (void *) XMAudioUtils_release },
};

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    JNIEnv* env = NULL;

    g_jvm = vm;
    if ((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        return -1;
    }
    assert(env != NULL);

    pthread_mutex_init(&g_clazz.mutex, NULL);

    IJK_FIND_JAVA_CLASS(env, g_clazz.clazz, JNI_CLASS_AUDIO_UTILS);
    (*env)->RegisterNatives(env, g_clazz.clazz, g_methods, NELEM(g_methods));

    g_clazz.field_mNativeXmAudioUtils = (*env)->GetFieldID(env, g_clazz.clazz, "mNativeXmAudioUtils", "J");

    RegisterFFmpeg();

    xm_audio_generator_global_init(env);
    ijksdl_android_global_init(g_jvm, env);
    return JNI_VERSION_1_4;
}

JNIEXPORT void JNI_OnUnload(JavaVM *jvm, void *reserved)
{
    xm_audio_generator_global_uninit();
    ijksdl_android_global_uninit();
    pthread_mutex_destroy(&g_clazz.mutex);
}
