#include "xm_audio_effects.h"
#include <string.h>
#include "log.h"
#include "tools/util.h"
#include "codec/ffmpeg_utils.h"

#define DEFAULT_CHANNEL_NUMBER 1

static void ae_free(XmEffectContext *ctx) {
    LogInfo("%s\n", __func__);
    if (NULL == ctx)
        return;

    for (short i = 0; i < MAX_NB_EFFECTS; ++i) {
        if (ctx->effects[i]) {
            free_effect(ctx->effects[i]);
            ctx->effects[i] = NULL;
        }
    }

    for (int i = 0; i < NB_BUFFERS; i++) {
        if (ctx->buffer[i]) {
            free(ctx->buffer[i]);
            ctx->buffer[i] = NULL;
        }
    }

    if (ctx->audio_fifo) {
        fifo_delete(&ctx->audio_fifo);
    }
}

static void flush(XmEffectContext *ctx) {
    LogInfo("%s start.\n", __func__);
    if (!ctx)
        return;

    short *buffer = ctx->buffer[EffectsPcm];
    while (1) {
        int receive_len = 0;
        for (int i = 0; i < MAX_NB_EFFECTS; ++i) {
            if (NULL == ctx->effects[i]) {
                continue;
            }

            bool is_last_effect = true;
            receive_len = receive_samples(ctx->effects[i],
                buffer, MAX_NB_SAMPLES);
            while (receive_len > 0) {
                for (short j = i + 1; j < MAX_NB_EFFECTS; ++j) {
                    if (NULL != ctx->effects[j]) {
                        receive_len = send_samples(ctx->effects[j],
                            buffer, receive_len);
                        if (receive_len < 0) {
                            LogError("%s send_samples to the next effect failed\n",__func__);
                            goto end;
                        }
                        is_last_effect = false;
                        receive_len = receive_samples(ctx->effects[i],
                            buffer, MAX_NB_SAMPLES);
                        break;
                    }
                }
                if (is_last_effect) break;
            }
        }

        if (receive_len > 0) {
            if (ctx->dst_channels == 2) {
                MonoToStereoS16(ctx->buffer[FifoPcm], buffer, receive_len);
                receive_len = receive_len << 1;
                buffer = ctx->buffer[FifoPcm];
            }
            int ret = fifo_write(ctx->audio_fifo, buffer, receive_len);
            if (ret < 0) goto end;
        } else {
            goto end;
        }
    }
end:
    LogInfo("%s end.\n", __func__);
    ctx->flush = true;
    return;
}

static int add_effects(XmEffectContext *ctx, short *buffer, int buffer_len) {
    int ret = -1;
    if (!ctx || !buffer || buffer_len <= 0) return -1;

    bool find_valid_effect = false;
    for (int i = 0; i < MAX_NB_EFFECTS; ++i) {
        if (NULL != ctx->effects[i]) {
            if(send_samples(ctx->effects[i], buffer, buffer_len) < 0) {
                LogError("%s send_samples to the first effect failed\n",
                    __func__);
                ret = -1;
                return ret;
            }
            find_valid_effect = true;
            break;
        }
    }
    if (!find_valid_effect) return buffer_len;

    int receive_len = 0;
    for (int i = 0; i < MAX_NB_EFFECTS; ++i) {
        if (NULL == ctx->effects[i]) {
            continue;
        }

        bool is_last_effect = true;
        receive_len = receive_samples(ctx->effects[i],
            buffer, MAX_NB_SAMPLES);
        while (receive_len > 0) {
            for (short j = i + 1; j < MAX_NB_EFFECTS; ++j) {
                if (NULL != ctx->effects[j]) {
                    receive_len = send_samples(ctx->effects[j],
                        buffer, receive_len);
                    if (receive_len < 0) {
                        LogError("%s send_samples to the next effect failed\n",
                            __func__);
                        ret = -1;
                        return ret;
                    }
                    is_last_effect = false;
                    receive_len = receive_samples(ctx->effects[i],
                        buffer, MAX_NB_SAMPLES);
                    break;
                }
            }
            if (is_last_effect) break;
        }
    }

    ret = receive_len;
    return ret;
}

static int read_pcm_frame(XmEffectContext *ctx, short *buffer) {
    if (!ctx || !buffer || !ctx->decoder) return -1;

    int read_len = MAX_NB_SAMPLES;
    return IAudioDecoder_get_pcm_frame(ctx->decoder,
        buffer, read_len, false);
}

static int add_effects_and_write_fifo(XmEffectContext *ctx) {
    int ret = -1, read_len = 0;
    short *buffer = ctx->buffer[RawPcm];
    if (!ctx || !ctx->audio_fifo) return -1;

    if ((ret = read_pcm_frame(ctx, buffer)) < 0) {
        if (ret != PCM_FILE_EOF)
            LogError("%s read_pcm_frame failed.\n", __func__);
        return ret;
    }

    if (ctx->decoder->out_nb_channels == 2) {
        read_len = ret >> 1;
        StereoToMonoS16(ctx->buffer[EffectsPcm], buffer, read_len);
        buffer = ctx->buffer[EffectsPcm];
    } else if(ctx->decoder->out_nb_channels == 1) {
        read_len = ret;
    }

    if ((ret = add_effects(ctx, buffer, read_len)) < 0) {
        LogError("%s add_effects failed.\n", __func__);
        return ret;
    }

    if (ctx->dst_channels == 2) {
        MonoToStereoS16(ctx->buffer[FifoPcm], buffer, ret);
        read_len = ret << 1;
        buffer = ctx->buffer[FifoPcm];
    } else if(ctx->dst_channels == 1) {
        read_len = ret;
    }

    ret = fifo_write(ctx->audio_fifo, buffer, read_len);
    return ret;
}

static int voice_effects_init(XmEffectContext *ctx,
    char **effects_info, int dst_sample_rate, int dst_channels) {
    LogInfo("%s\n", __func__);
    if (!ctx || !effects_info)
        return -1;

    for (int i = 0; i < MAX_NB_EFFECTS; ++i) {
        if (ctx->effects[i]) {
            free_effect(ctx->effects[i]);
            ctx->effects[i] = NULL;
        }
    }

    for (int i = 0; i < MAX_NB_EFFECTS; ++i) {
        if (NULL == effects_info[i]) continue;

        switch (i) {
            case NoiseSuppression:
                ctx->effects[NoiseSuppression] = create_effect(
                    find_effect("noise_suppression"), dst_sample_rate,
                    dst_channels);
                init_effect(ctx->effects[NoiseSuppression], 0, NULL);
                set_effect(ctx->effects[NoiseSuppression], "Switch",
                    effects_info[i], 0);
                break;
            case Beautify:
                ctx->effects[Beautify] = create_effect(
                    find_effect("beautify"), dst_sample_rate,
                    dst_channels);
                init_effect(ctx->effects[Beautify], 0, NULL);
                set_effect(ctx->effects[Beautify], "mode",
                    effects_info[i], 0);
                break;
            case Reverb:
                ctx->effects[Reverb] = create_effect(
                    find_effect("reverb"), dst_sample_rate,
                    dst_channels);
                init_effect(ctx->effects[Reverb], 0, NULL);
                set_effect(ctx->effects[Reverb], "mode",
                    effects_info[i], 0);
                break;
            case VolumeLimiter:
                ctx->effects[VolumeLimiter] = create_effect(
                    find_effect("limiter"), dst_sample_rate,
                    dst_channels);
                init_effect(ctx->effects[VolumeLimiter], 0, NULL);
                set_effect(ctx->effects[VolumeLimiter], "Switch",
                    effects_info[i], 0);
                break;
            case Minions:
                ctx->effects[Minions] = create_effect(
                    find_effect("minions"), dst_sample_rate,
                    dst_channels);
                init_effect(ctx->effects[Minions], 0, NULL);
                set_effect(ctx->effects[Minions], "Switch",
                    effects_info[i], 0);
                break;
            case VoiceMorph:
                ctx->effects[VoiceMorph] = create_effect(
                    find_effect("voice_morph"), dst_sample_rate,
                    dst_channels);
                init_effect(ctx->effects[VoiceMorph], 0, NULL);
                set_effect(ctx->effects[VoiceMorph], "mode",
                    effects_info[i], 0);
                break;
            default:
                LogWarning("%s unsupported effect %s\n", __func__,
                    effects_info[i]);
                break;
        }
    }

    return 0;
}

static int init(XmEffectContext *ctx, char **effects_info)
{
    int ret = -1;
    if (!ctx) return ret;

    ae_free(ctx);
    ctx->flush = false;
    IAudioDecoder *decoder = ctx->decoder;

    if ((ret = voice_effects_init(ctx, effects_info,
        decoder->out_sample_rate, DEFAULT_CHANNEL_NUMBER)) < 0) {
        LogError("%s voice_effects_init failed.\n", __func__);
        goto fail;
    }

    for (int i = 0; i < NB_BUFFERS; i++) {
        ctx->buffer[i] = (short *)calloc(sizeof(short), 2 * MAX_NB_SAMPLES);
        if (!ctx->buffer[i]) {
            LogError("%s calloc buffer[%d] failed.\n", __func__, i);
            ret = AEERROR_NOMEM;
            goto fail;
        }
    }

    // Allocate buffer for audio fifo
    ctx->audio_fifo = fifo_create((size_t)(decoder->out_bits_per_sample >> 3));
    if (!ctx->audio_fifo) {
        LogError("%s Could not allocate audio FIFO\n", __func__);
        ret = AEERROR_NOMEM;
        goto fail;
    }

    ret = 0;
fail:
    return ret;
}

void audio_effect_freep(XmEffectContext **ctx) {
    LogInfo("%s\n", __func__);
    if (NULL == ctx || NULL == *ctx)
        return;

    ae_free(*ctx);
    free(*ctx);
    *ctx = NULL;
}

int audio_effect_get_frame(XmEffectContext *ctx,
    short *buffer, int buffer_size_in_short) {
    int ret = -1;
    if (!ctx || !buffer || buffer_size_in_short <= 0)
        return ret;

    while (fifo_occupancy(ctx->audio_fifo) < (size_t) buffer_size_in_short) {
        ret = add_effects_and_write_fifo(ctx);
        if (ret < 0) {
            if (!ctx->flush) flush(ctx);
            if (0 < fifo_occupancy(ctx->audio_fifo)) {
                break;
            } else {
                goto end;
            }
        }
    }

    return fifo_read(ctx->audio_fifo, buffer, buffer_size_in_short);
end:
    return ret;
}

int audio_effect_init(XmEffectContext *ctx,
    IAudioDecoder *decoder, char **effects_info, int dst_channels) {
    if (!ctx || !decoder || !effects_info)
        return -1;

    ctx->decoder = decoder;
    ctx->dst_channels = dst_channels;
    return init(ctx, effects_info);
}

XmEffectContext *audio_effect_create() {
    XmEffectContext *self =
        (XmEffectContext *)calloc(1, sizeof(XmEffectContext));
    if (NULL == self) {
        LogError("%s alloc XmEffectContext failed.\n", __func__);
        return NULL;
    }

    return self;
}
