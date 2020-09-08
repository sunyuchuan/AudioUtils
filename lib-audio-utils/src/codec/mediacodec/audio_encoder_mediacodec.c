#if defined(__ANDROID__)

#include "audio_encoder_mediacodec.h"
#include "libavutil/frame.h"
#include "libavutil/imgutils.h"
#include "libavcodec/avcodec.h"
#include "xm_android_jni.h"
#include "ijksdl/ijksdl_codec_android_mediaformat_java.h"
#include "ijksdl/ijksdl_codec_android_mediacodec_java.h"
#include "ijkj4a/j4a_base.h"
#include "ijkj4a/MediaCodec.h"
#include "error_def.h"
#include "log.h"

#define AMC_ENCODER_INPUT_TIMEOUT_US  -1
#define AMC_ENCODER_OUTPUT_TIMEOUT_US 0

typedef struct Encoder_Opaque {
    SDL_AMediaFormat  *input_aformat;
    SDL_AMediaCodec   *acodec;
    FF_AVMediaType    type;
    int               width;
    int               height;
    int               pix_fmt;
    uint8_t           *raw_buf;
    int               raw_buf_size;
    int               flush_encoder;
    int               convert_rate;
    int               time_gap;
    uint64_t          time_stamp;
} Encoder_Opaque;

static int mediacodec_encoder_get_frame_size(Encoder *encoder)
{
    LogInfo("%s.\n", __func__);
    int ret = -1;
    JNIEnv *env = NULL;
    Encoder_Opaque *opaque = encoder->opaque;
    jobject input_buffer_array = NULL;
    jobject input_buffer = NULL;

    if (JNI_OK != SDL_JNI_SetupThreadEnv(&env)) {
        LogError("%s:create: SetupThreadEnv failed.\n", __func__);
        goto fail;
    }
    jobject android_media_codec = SDL_AMediaCodecJava_getObject(env, opaque->acodec);

    input_buffer_array = J4AC_MediaCodec__getInputBuffers__catchAll(env, android_media_codec);
    if (!input_buffer_array)
        return -1;

    int buffer_count = (*env)->GetArrayLength(env, input_buffer_array);
    if (J4A_ExceptionCheck__catchAll(env) || buffer_count <= 0) {
        LogError("get buffer count error.\n");
        goto fail;
    }

    input_buffer = (*env)->GetObjectArrayElement(env, input_buffer_array, 0);
    if (J4A_ExceptionCheck__catchAll(env) || !input_buffer) {
        LogError("%s: GetObjectArrayElement failed.\n", __func__);
        goto fail;
    }
    jlong buf_size = (*env)->GetDirectBufferCapacity(env, input_buffer);
    ret = buf_size;
fail:
    SDL_JNI_DeleteLocalRefP(env, &input_buffer);
    SDL_JNI_DeleteLocalRefP(env, &input_buffer_array);
    return ret;
}

static int mediacodec_encoder_config(Encoder *encoder, AVDictionary *opt)
{
    LogInfo("%s.\n", __func__);
    if (!encoder || !encoder->opaque)
        return -1;

    Encoder_Opaque *opaque = encoder->opaque;
    JNIEnv           *env  = NULL;
    AVDictionaryEntry *e = NULL;
    int amc_ret = 0;
    int ret = 0;

    if (JNI_OK != SDL_JNI_SetupThreadEnv(&env)) {
        LogError("%s:create: SetupThreadEnv failed.\n", __func__);
        goto fail;
    }

    if (opt == NULL) {
        LogError("config encoder pls set opt !");
        goto fail;
    }

    //find codec type
    while ((e = av_dict_get(opt, "", e, AV_DICT_IGNORE_SUFFIX)) != NULL) {
        if (!strcasecmp(e->key, "mime")) {
            if (!strcasecmp(e->value, MIME_VIDEO_AVC)) {
                opaque->type = FF_AVMEDIA_TYPE_VIDEO;
                break;
            } else if (!strcasecmp(e->value, MIME_AUDIO_AAC)){
                opaque->type = FF_AVMEDIA_TYPE_AUDIO;
                break;
            } else {
                opaque->type = FF_AVMEDIA_TYPE_UNKNOWN;
                LogError("unsupport mime %s!\n", e->value);
                return -1;
            }
        }
    }

    SDL_AMediaCodec *acodec = NULL;
    SDL_AMediaFormat *aformat = NULL;
    if (opaque->type == FF_AVMEDIA_TYPE_AUDIO) {
        //default config
        int64_t bit_rate     = 128000;
        int32_t channels     = 2;
        int32_t sample_rate  = 44100;
        __unused uint64_t channel_layout = av_get_default_channel_layout(channels);
        acodec = SDL_AMediaCodecJava_createEncoderByType(env, SDL_AMIME_AUDIO_RAW_AAC);
        if (!acodec) {
            LogError("SDL_AMediaCodecJava_createEncoderByType failed.\n");
            goto fail;
        }
        aformat = SDL_AMediaFormatJava_createFormat(env);
        if (!aformat) {
            LogError("SDL_AMediaFormatJava_createFormat failed.\n");
            goto fail;
        }

        SDL_AMediaFormat_setString(aformat, AMEDIAFORMAT_KEY_MIME, SDL_AMIME_AUDIO_RAW_AAC);
        while ((e = av_dict_get(opt, "", e, AV_DICT_IGNORE_SUFFIX)) != NULL) {
            if (!strcasecmp(e->key, "bit_rate")) {
                bit_rate = atoll(e->value);
            } else if (!strcasecmp(e->key, "sample_rate")) {
                sample_rate = atoi(e->value);
            } else if (!strcasecmp(e->key, "channels")) {
                channels = atoi(e->value);
            } else if (!strcasecmp(e->key, "channel_layout")) {
                channel_layout = atoll(e->value);
            }
        }

        SDL_AMediaFormat_setInt32(aformat, AMEDIAFORMAT_KEY_SAMPLE_RATE, sample_rate);
        SDL_AMediaFormat_setInt32(aformat, AMEDIAFORMAT_KEY_BIT_RATE, (int32_t)bit_rate);
        SDL_AMediaFormat_setInt32(aformat, AMEDIAFORMAT_KEY_CHANNEL_COUNT, channels);
        SDL_AMediaFormat_setInt32(aformat, AMEDIAFORMAT_KEY_AAC_PROFILE, 2);
        //fixme: todo 
        //channel_layout in android is called channel_mask
        LogInfo("audio encoder sample rate %d bit rate %"PRId64" channel %d.\n", sample_rate, bit_rate, channels);
    } else if (opaque->type == FF_AVMEDIA_TYPE_VIDEO) {
        int64_t bit_rate = 0;
        int width = 0;
        int height = 0;
        int gop_size = 0;
        int pix_fmt = 0;
        int frame_rate = 0;
        int hw_pix_fmt = AMEDIACODEC__OMX_COLOR_FormatYUV420SemiPlanar;
        acodec = SDL_AMediaCodecJava_createEncoderByType(env, SDL_AMIME_VIDEO_AVC);
        if (!acodec) {
            LogError("SDL_AMediaCodecJava_createEncoderByType failed.\n");
            goto fail;
        }
        aformat = SDL_AMediaFormatJava_createFormat(env);
        if (!aformat) {
            LogError("SDL_AMediaFormatJava_createFormat failed.\n");
            goto fail;
        }

        SDL_AMediaFormat_setString(aformat, AMEDIAFORMAT_KEY_MIME, SDL_AMIME_VIDEO_AVC);
        while ((e = av_dict_get(opt, "", e, AV_DICT_IGNORE_SUFFIX)) != NULL) {
            if (!strcasecmp(e->key, "bit_rate")) {
                bit_rate = atoll(e->value);
            } else if (!strcasecmp(e->key, "width")) {
                width = atoi(e->value);
            } else if (!strcasecmp(e->key, "height")) {
                height = atoi(e->value);
            } else if (!strcasecmp(e->key, "gop_size")) {
                gop_size = atoi(e->value);
            } else if (!strcasecmp(e->key, "pix_format")) {
                pix_fmt = atoi(e->value);
            } else if (!strcasecmp(e->key, "framerate")) {            
                frame_rate = atoi(e->value);
            }
        }
        if (pix_fmt == AV_PIX_FMT_YUV420P) {
            hw_pix_fmt = AMEDIACODEC__OMX_COLOR_FormatYUV420Planar;
        } else if (pix_fmt == AV_PIX_FMT_NV12) {
            hw_pix_fmt = AMEDIACODEC__OMX_COLOR_FormatYUV420SemiPlanar;
        } else {
            //howtodo?
            LogWarning("unknown color format %d.\n", pix_fmt);
        }
        //SDL_AMediaFormat_setInt32(aformat, AMEDIAFORMAT_KEY_COLOR_FORMAT, AMEDIACODEC__OMX_COLOR_FormatYUV420Flexible);
        SDL_AMediaFormat_setInt32(aformat, AMEDIAFORMAT_KEY_COLOR_FORMAT, hw_pix_fmt);
        SDL_AMediaFormat_setInt32(aformat, AMEDIAFORMAT_KEY_BIT_RATE, (int32_t)bit_rate);
        SDL_AMediaFormat_setInt32(aformat, AMEDIAFORMAT_KEY_WIDTH, width);
        SDL_AMediaFormat_setInt32(aformat, AMEDIAFORMAT_KEY_HEIGHT, height);
        SDL_AMediaFormat_setInt32(aformat, AMEDIAFORMAT_KEY_FRAME_RATE, frame_rate);
        SDL_AMediaFormat_setInt32(aformat, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, gop_size / frame_rate);
        //hardcode nv12, get video frame buffer size
        opaque->width = width;
        opaque->height = height;
        opaque->pix_fmt = pix_fmt;

        //opaque->raw_buf_size = av_image_get_buffer_size(AV_PIX_FMT_NV12, width, height, 1);
        LogInfo("video encoder bitrate %"PRId64" w x h %dx%d framerate %d gopsize %d.\n", bit_rate, width, height, frame_rate, gop_size);
    }

    amc_ret = SDL_AMediaCodec_configure_surface(env, acodec, aformat, NULL, NULL, AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
    if (amc_ret != SDL_AMEDIA_OK) {
        LogError("%s:configure_surface: failed.\n", __func__);
        ret = -1;
        goto fail;
    }

    amc_ret = SDL_AMediaCodec_start(acodec);
    if (amc_ret != SDL_AMEDIA_OK) {
        LogError("%s:SDL_AMediaCodec_start: failed.\n", __func__);
        ret = -1;
        goto fail;
    }

    opaque->acodec = acodec;
    opaque->input_aformat = aformat;
    opaque->raw_buf_size = mediacodec_encoder_get_frame_size(encoder);
    opaque->raw_buf = av_mallocz(opaque->raw_buf_size);
    if (opaque->type == FF_AVMEDIA_TYPE_AUDIO) {
        int32_t channels = 2;
        int32_t convert_rate = 44100;
        SDL_AMediaFormat_getInt32(aformat, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &channels);
        SDL_AMediaFormat_getInt32(aformat, AMEDIAFORMAT_KEY_SAMPLE_RATE, &convert_rate);
        opaque->time_gap = mediacodec_encoder_get_frame_size(encoder) / channels / 2;
        opaque->convert_rate = convert_rate;
    } else {
        int32_t convert_rate = 25;
        SDL_AMediaFormat_getInt32(aformat, AMEDIAFORMAT_KEY_FRAME_RATE, &convert_rate);
        opaque->convert_rate = convert_rate;
        opaque->time_gap = 1;
    }
    LogInfo("raw buffer size %d.\n", opaque->raw_buf_size);

fail:
    return ret;
}

static int mediacodec_encoder_encode_frame(Encoder *encoder, AVFrame *frame, AVPacket *pkt, int *got_packet_ptr)
{
    int ret = -1;
    Encoder_Opaque *opaque = encoder->opaque;
    SDL_AMediaCodecBufferInfo bufferInfo;
    ssize_t  input_buffer_index = AMEDIACODEC__UNKNOWN_ERROR;
    ssize_t  copy_size          = 0;
    uint32_t queue_flags        = 0;
    ssize_t  output_buffer_index = 0;
    uint8_t *alldata = NULL;
    uint32_t allsize = 0;

    //wait input buffer available block
    if (!opaque->flush_encoder)
        input_buffer_index = SDL_AMediaCodec_dequeueInputBuffer(opaque->acodec, AMC_ENCODER_INPUT_TIMEOUT_US);

    if (input_buffer_index >= 0) {
        if (frame != NULL) {
            opaque->time_stamp = frame->pts;
            //need trans frame to buffer
            if (opaque->type == FF_AVMEDIA_TYPE_VIDEO) {
                int number_of_written_bytes = av_image_copy_to_buffer(opaque->raw_buf, opaque->raw_buf_size,
                                    (const uint8_t* const *)frame->data, (const int*) frame->linesize,
                                    opaque->pix_fmt, opaque->width, opaque->height, 1);
                if (number_of_written_bytes < 0) {
                    LogError("Can't copy image to buffer.\n");
                    return number_of_written_bytes;
                }
            } else if (opaque->type == FF_AVMEDIA_TYPE_AUDIO) {
                memcpy(opaque->raw_buf, frame->data[0], frame->linesize[0]);
            }
            copy_size = SDL_AMediaCodec_writeInputData(opaque->acodec, input_buffer_index, (const uint8_t *)opaque->raw_buf, opaque->raw_buf_size);
            if (!copy_size) {
                LogInfo("%s: SDL_AMediaCodec_writeInputData failed.\n", __func__);
                ret = -1;
                goto fail;
            }
        } else {
            //end of stream
            copy_size = 0;
            queue_flags = AMEDIACODEC__BUFFER_FLAG_END_OF_STREAM;
            opaque->flush_encoder = 1;
            opaque->time_stamp += opaque->time_gap;
            LogInfo("end of stream.\n");
        }
        ret = SDL_AMediaCodec_queueInputBuffer(opaque->acodec, input_buffer_index, 0, copy_size,
                                               opaque->time_stamp * 1000000 / opaque->convert_rate, queue_flags);
        if (ret != SDL_AMEDIA_OK) {
            LogError("%s: SDL_AMediaCodec_getInputBuffer failed.\n", __func__);
            ret = -1;
            goto fail;
        }
    } else if (input_buffer_index == AMEDIACODEC__INFO_TRY_AGAIN_LATER) {
        //how to do?
        LogWarning("input buffer try again later !!!!\n");
        //return -1;
    }

    output_buffer_index = SDL_AMediaCodec_dequeueOutputBuffer(opaque->acodec, &bufferInfo, AMC_ENCODER_OUTPUT_TIMEOUT_US);
    uint8_t *data = NULL;
    if (output_buffer_index >= 0) {
        SDL_AMediaCodec_readOutputData(opaque->acodec, output_buffer_index, &data);
        if (bufferInfo.size > 0 && data) {
            alldata = (uint8_t *)av_malloc(bufferInfo.size);
            memcpy(alldata, data + bufferInfo.offset, bufferInfo.size);
            free(data);
            ret = bufferInfo.size;
            allsize = bufferInfo.size;
            *got_packet_ptr = 1;
            pkt->size = allsize;
            if (opaque->type == FF_AVMEDIA_TYPE_AUDIO)
                pkt->pts = pkt->dts = bufferInfo.presentationTimeUs * opaque->convert_rate / 1000000;
            else
                pkt->pts = bufferInfo.presentationTimeUs * opaque->convert_rate / 1000000;
            //av_log(NULL, AV_LOG_INFO, "buffer size %d", allsize);
            av_packet_from_data(pkt, alldata, allsize);
        }
        SDL_AMediaCodec_releaseOutputBuffer(opaque->acodec, output_buffer_index, false);
        if ((bufferInfo.flags & AMEDIACODEC__BUFFER_FLAG_END_OF_STREAM) != 0) {
            LogInfo("end of stream output buffer.\n");
            *got_packet_ptr = 0;
            ret = 0;
        } else if ((bufferInfo.flags & AMEDIACODEC__BUFFER_FLAG_KEY_FRAME) != 0) {
            pkt->flags |= AV_PKT_FLAG_KEY;
        } else if ((bufferInfo.flags & AMEDIACODEC__BUFFER_FLAG_CODEC_CONFIG) != 0) {
            *got_packet_ptr = 0;
            ret = 0;
        }
    } else if (output_buffer_index == AMEDIACODEC__INFO_OUTPUT_FORMAT_CHANGED ||
            output_buffer_index == AMEDIACODEC__INFO_TRY_AGAIN_LATER) {
        *got_packet_ptr = 0;
        ret = 0;
        LogWarning("output buffer state change code 0x%zx.\n", output_buffer_index);
    } else {
        *got_packet_ptr = 0;
        ret = -1;
        LogError("mediacodec state error can't encoder a packet ret 0x%zx.\n", output_buffer_index);
    }

fail:
    return ret;
}

static int mediacodec_encoder_destroy(Encoder *encoder)
{
    LogInfo("%s.\n", __func__);
    if (!encoder || !encoder->opaque)
        return -1;

    Encoder_Opaque *opaque = encoder->opaque;
    av_freep(&opaque->raw_buf);
    SDL_AMediaCodec_decreaseReferenceP(&opaque->acodec);
    SDL_AMediaFormat_deleteP(&opaque->input_aformat);

    return 0;
}

Encoder *ff_encoder_mediacodec_create()
{
    LogInfo("%s.\n", __func__);
    if (SDL_Android_GetApiLevel() < IJK_API_16_JELLY_BEAN)
        return NULL;

    Encoder *encoder = ff_encoder_alloc(sizeof(Encoder_Opaque));
    if (!encoder)
        return encoder;

    Encoder_Opaque *opaque = encoder->opaque;

    opaque->type = FF_AVMEDIA_TYPE_UNKNOWN;

    encoder->func_config  = mediacodec_encoder_config;
    encoder->func_get_frame_size = mediacodec_encoder_get_frame_size;
    encoder->func_encode_frame = mediacodec_encoder_encode_frame;
    encoder->func_destroy = mediacodec_encoder_destroy;

    return encoder;
}
#endif
