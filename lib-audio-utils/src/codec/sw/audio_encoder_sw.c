#if defined(__ANDROID__) || defined (__linux__)

#include <stdbool.h>
#include <strings.h>
#include "audio_encoder_sw.h"
#include "libavutil/frame.h"
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "error_def.h"
#include "log.h"
#include "codec/ffmpeg_utils.h"

typedef struct Encoder_Opaque {
    AVCodecContext *codec_ctx;
    AVCodec *codec;
    int frame_byte_size;
    FF_AVMediaType type;
    AVFrame *frame;
} Encoder_Opaque;

static int sw_encoder_config(Encoder *encoder, AVDictionary *opt) {
    LogInfo("%s.\n", __func__);
    int ret = -1;
    Encoder_Opaque *opaque = encoder->opaque;
    AVDictionaryEntry *e = NULL;
    enum AVCodecID codec_id = AV_CODEC_ID_NONE;

    //find codec type
    while ((e = av_dict_get(opt, "", e, AV_DICT_IGNORE_SUFFIX)) != NULL) {
        if (!strcasecmp(e->key, "mime")) {
            if (!strcasecmp(e->value, MIME_VIDEO_AVC)) {
                codec_id = AV_CODEC_ID_H264;
                opaque->type = FF_AVMEDIA_TYPE_VIDEO;
                break;
            } else if (!strcasecmp(e->value, MIME_AUDIO_AAC)){
                codec_id = AV_CODEC_ID_AAC;
                opaque->type = FF_AVMEDIA_TYPE_AUDIO;
                break;
            } else if (!strcasecmp(e->value, MIME_AUDIO_WAV)){
                codec_id = AV_CODEC_ID_PCM_S16LE;
                opaque->type = FF_AVMEDIA_TYPE_AUDIO;
                break;
            } else {
                opaque->type = FF_AVMEDIA_TYPE_UNKNOWN;
                LogError("unsupport mime %s!\n", e->value);
                return ret;
            }
        }
    }

    /* find the encoder */
    AVCodec *codec = avcodec_find_encoder(codec_id);
    if (!codec) {
       LogError("Codec not found.\n");
       return ret;
    }

    AVCodecContext *ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        LogError("Could not allocate audio codec context.\n");
        return ret;
    }

    if (opaque->type == FF_AVMEDIA_TYPE_AUDIO) {
        //audio encoder config
        while ((e = av_dict_get(opt, "", e, AV_DICT_IGNORE_SUFFIX)) != NULL) {
            if (!strcasecmp(e->key, "bit_rate")) {
                ctx->bit_rate = atoll(e->value);
            } else if (!strcasecmp(e->key, "sample_rate")) {
                ctx->sample_rate = atoi(e->value);
            } else if (!strcasecmp(e->key, "channels")) {
                ctx->channels = atoi(e->value);
            } else if (!strcasecmp(e->key, "channel_layout")) {
                ctx->channel_layout = atoll(e->value);
            }
        }

        ctx->cutoff = 18000;
        ctx->sample_fmt =
                codec->sample_fmts ? codec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
        LogInfo("sample fmt %x.\n", ctx->sample_fmt);
        bool supported_samplerates = false;
        if (codec->supported_samplerates) {
            for (int i = 0; codec->supported_samplerates[i]; i++) {
                if (codec->supported_samplerates[i] == ctx->sample_rate)
                    supported_samplerates = true;
            }
            if (false == supported_samplerates) {
                LogError("The codec doesn't support the sample rate.\n");
                ret = kSampleRateNotSupport;
                return ret;
            }
        }
        ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
        if (!ctx->channel_layout) {
            ctx->channel_layout = av_get_default_channel_layout(ctx->channels);
        }
        ctx->time_base = (AVRational){1, ctx->sample_rate};
        LogInfo("sw sample rate %d bit rate %" PRId64 " channel %d.\n", ctx->sample_rate, ctx->bit_rate, ctx->channels);
    } else if(opaque->type == FF_AVMEDIA_TYPE_VIDEO) {
        //video encoder config
        //default value
        int framerate = 25;
        int multiple = 1;
        while ((e = av_dict_get(opt, "", e, AV_DICT_IGNORE_SUFFIX)) != NULL) {
            if (!strcasecmp(e->key, "bit_rate")) {
                ctx->bit_rate = atoll(e->value);
            } else if (!strcasecmp(e->key, "width")) {
                ctx->width = atoi(e->value);
            } else if (!strcasecmp(e->key, "height")) {
                ctx->height = atoi(e->value);
            } else if (!strcasecmp(e->key, "gop_size")) {
                ctx->gop_size = atoi(e->value);
            } else if (!strcasecmp(e->key, "pix_format")) {
                ctx->pix_fmt = atoi(e->value);
            } else if (!strcasecmp(e->key, "framerate")) {
                framerate = atoi(e->value);
                ctx->framerate = (AVRational){ framerate, 1 };
            } else if (!strcasecmp(e->key, "preset")) {
                av_opt_set(ctx->priv_data, "preset", e->value, 0);
            } else if (!strcasecmp(e->key, "tune")) {
                av_opt_set(ctx->priv_data, "tune", e->value, 0);
            } else if (!strcasecmp(e->key, "crf")) {
                av_opt_set_double(ctx->priv_data, "crf", (double)atoi(e->value), 0);
            } else if (!strcasecmp(e->key, "multiple")) {
                multiple = atoi(e->value);
            } else if (!strcasecmp(e->key, "max_b_frames")) {
                ctx->max_b_frames = atoi(e->value);
            }
        }
        //av_opt_set(ctx->priv_data, "preset", "slow", 0);
        ctx->time_base = (AVRational){1, framerate * multiple};
        LogInfo("sw bitrate %" PRId64 " w x h %dx%d framerate %d gopsize %d.\n", ctx->bit_rate, ctx->width, ctx->height, framerate, ctx->gop_size);
    } else {
        LogError("unknown media type !!!\n");
    }

    /* open it */
    if (avcodec_open2(ctx, codec, NULL) < 0) {
       LogError("Could not open codec.\n");
       return -1;
    }

    //need convert s16 to float alloc a temp audio frame
    if (opaque->type == FF_AVMEDIA_TYPE_AUDIO) {
        opaque->frame_byte_size = ctx->frame_size * ctx->channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
        opaque->frame = av_frame_alloc();

        if (opaque->frame == NULL) {
            LogError("Could not allocate frame%s.\n", av_err2str(ret));
            goto fail;
        }
        if (ctx->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE) {
            opaque->frame->nb_samples = FRAME_SIZE;
            opaque->frame_byte_size = FRAME_SIZE * ctx->channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
        } else {
            opaque->frame->nb_samples = ctx->frame_size;
        }
        opaque->frame->format         = ctx->sample_fmt;
        opaque->frame->channel_layout = ctx->channel_layout;
        ret = av_frame_get_buffer(opaque->frame, 0);
        if (ret < 0) {
           LogError("Could not allocate audio data buffers %s.\n", av_err2str(ret));
           av_frame_free(&opaque->frame);
           goto fail;
        }
    }
    opaque->codec_ctx = ctx;
    opaque->codec = codec;
    return 0;

fail:
    avcodec_free_context(&ctx);
    return ret;
}

static int sw_encoder_get_frame_size(Encoder *encoder)
{
    Encoder_Opaque *opaque = encoder->opaque;
    LogInfo("frame size in byte %d.\n", opaque->frame_byte_size);
    return opaque->frame_byte_size;
}

static void S16toFLTP(int16_t* in, float** out, int size, int out_channels)
{
    for (int i = 0; i < size; i++) {
        if (out_channels == 2) {
            ((float**)out)[i % 2][i >> 1] = (float)(in[i]) / 32768.0f;
        } else if (out_channels == 1) {
            ((float**)out)[0][i] = (float)(in[i]) / 32768.0f;
        } else {
            LogError("out_channels invalid.\n");
        }
    }
}

static int sw_encoder_encode_frame(Encoder *encoder, AVFrame *frame, AVPacket *pkt, int *got_packet_ptr) {
    Encoder_Opaque *opaque = encoder->opaque;
    AVCodecContext *ctx = opaque->codec_ctx;
    int ret = -1;
    if (opaque->type == FF_AVMEDIA_TYPE_AUDIO) {
        if (frame != NULL && frame->format == AV_SAMPLE_FMT_S16) {
            if (opaque->codec->id == AV_CODEC_ID_PCM_S16LE) {
                ret = avcodec_encode_audio2(ctx, pkt, frame, got_packet_ptr);
            } else {
                AVFrame *floatframe = opaque->frame;
                S16toFLTP((int16_t *)frame->data[0], (float **)floatframe->data,
                    ctx->frame_size * ctx->channels, ctx->channels);
                floatframe->format = AV_SAMPLE_FMT_FLTP;
                floatframe->pts = frame->pts;
                ret = avcodec_encode_audio2(ctx, pkt, floatframe, got_packet_ptr);
            }
        } else {
            ret = avcodec_encode_audio2(ctx, pkt, frame, got_packet_ptr);
        }
        return ret;
    } else if (opaque->type == FF_AVMEDIA_TYPE_VIDEO) {
        return avcodec_encode_video2(ctx, pkt, frame, got_packet_ptr);
    } else {
        return -1;
    }
}

static int sw_encoder_destroy(Encoder *encoder) {
    LogInfo("%s.\n", __func__);
    Encoder_Opaque *opaque = encoder->opaque;
    if (opaque) {
        if (opaque->frame) {
            av_frame_free(&(opaque->frame));
        }
        if (opaque->codec_ctx) {
            avcodec_free_context(&(opaque->codec_ctx));
        }
    }
    return 0;
}

Encoder *ff_encoder_sw_create() {
    LogInfo("%s.\n", __func__);
    Encoder *encoder = ff_encoder_alloc(sizeof(Encoder_Opaque));
    if (!encoder)
        return encoder;

    Encoder_Opaque *opaque = encoder->opaque;

    opaque->type = FF_AVMEDIA_TYPE_UNKNOWN;

    encoder->func_config  = sw_encoder_config;
    encoder->func_get_frame_size = sw_encoder_get_frame_size;
    encoder->func_encode_frame = sw_encoder_encode_frame;
    encoder->func_destroy = sw_encoder_destroy;

    return encoder;
}
#endif
