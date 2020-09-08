#include "xm_audio_mixer.h"
#include "codec/audio_decoder_factory.h"
#include "json/json_parse.h"
#include "codec/audio_muxer.h"
#include <pthread.h>
#include "mixer_effects.h"
#include "side_chain_compress.h"
#include "error_def.h"
#include "log.h"
#include "tools/util.h"
#include "tools/fifo.h"
#include "tools/conversion.h"
#include "effects/xm_audio_effects.h"

#define DEFAULT_SAMPLE_RATE 44100
#define DEFAULT_CHANNEL_NUMBER_2 2
#define DEFAULT_CHANNEL_NUMBER_1 1

struct XmMixerContext_T {
    volatile bool abort;
    int mix_status;
    int progress;
    // output sample rate and number channels
    int dst_sample_rate;
    int dst_channels;
    int bits_per_sample;
    // input pcm file seek position
    int seek_time_ms;
    // input pcm read location
    int64_t cur_size;
    fifo *audio_fifo;
    AudioMuxer *muxer;
    char *in_config_path;
    EffectContext *reverb_ctx;
    short *zero_buffer;
    pthread_mutex_t mutex;
    MixerEffects mixer_effects;
};

static void reverb_free(XmMixerContext *ctx) {
    LogInfo("%s\n", __func__);
    if (!ctx) return;

    if (ctx->reverb_ctx) {
        free_effect(ctx->reverb_ctx);
        ctx->reverb_ctx = NULL;
    }
}

static void mixer_effects_free(MixerEffects *mixer) {
    LogInfo("%s\n", __func__);
    if (NULL == mixer)
        return;

    for (int i = 0; i < MAX_NB_TRACKS; i++) {
        if (mixer->source[i]) {
            AudioSource_freep(&mixer->source[i]);
        }
        if (mixer->sourceQueue[i]) {
            AudioSourceQueue_freep(&mixer->sourceQueue[i]);
        }
        if (mixer->sourceQueueBackup[i]) {
            AudioSourceQueue_freep(&mixer->sourceQueueBackup[i]);
        }
    }
}

static int reverb_init(XmMixerContext *ctx) {
    int ret = -1;
    if (!ctx) return -1;

    if (ctx->reverb_ctx) {
        free_effect(ctx->reverb_ctx);
        ctx->reverb_ctx = NULL;
    }

    ctx->reverb_ctx = create_effect(find_effect("reverb"),
        ctx->dst_sample_rate, ctx->dst_channels);

    ret = init_effect(ctx->reverb_ctx, 0, NULL);
    if (ret < 0) goto fail;

    ret = set_effect(ctx->reverb_ctx, "Switch", "On", 0);
    if (ret < 0) goto fail;

    ret = set_effect(ctx->reverb_ctx, "reverb", REVERB_PARAMS, 0);
    if (ret < 0) goto fail;

    return ret;
fail:
    if (ctx->reverb_ctx) free_effect(ctx->reverb_ctx);
    ctx->reverb_ctx = NULL;
    return ret;
}

static int mixer_effects_init(MixerEffects *mixer) {
    LogInfo("%s\n", __func__);
    int ret = -1;
    if (NULL == mixer)
        return ret;

    mixer_effects_free(mixer);

    for (int i = 0; i < MAX_NB_TRACKS; i++) {
        mixer->source[i] = (AudioSource *)calloc(1, sizeof(AudioSource));
        if (NULL == mixer->source[i]) {
            LogError("%s alloc AudioSource failed.\n", __func__);
            ret = -1;
            goto fail;
        }

        mixer->sourceQueue[i] = AudioSourceQueue_create();
        if (NULL == mixer->sourceQueue[i]) {
            LogError("%s alloc sourceQueue failed.\n", __func__);
            ret = -1;
            goto fail;
        }

        mixer->sourceQueueBackup[i] = AudioSourceQueue_create();
        if (NULL == mixer->sourceQueueBackup[i]) {
            LogError("%s alloc sourceQueueBackup failed.\n", __func__);
            ret = -1;
            goto fail;
        }
    }

    ret = 0;
fail:
    return ret;
}

static int get_pcm_from_decoder(AudioSource *source,
    short *buffer, int buffer_size_in_short) {
    int ret = -1;
    if (!source || !source->decoder
        || !buffer || buffer_size_in_short <= 0)
        return ret;

    if(source->has_effects) {
        ret = audio_effect_get_frame(source->effects_ctx,
            buffer, buffer_size_in_short);
    } else {
        ret = IAudioDecoder_get_pcm_frame(source->decoder,
            buffer, buffer_size_in_short, source->is_loop);
    }

    return ret;
}

static IAudioDecoder *open_source_decoder(AudioSource *source,
        int dst_sample_rate, int dst_channels, int seek_time_ms) {
    LogInfo("%s\n", __func__);
    if (!source || !source->file_path)
        return NULL;
    IAudioDecoder *decoder = NULL;

    IAudioDecoder_freep(&(source->decoder));
    int decoder_out_channels = dst_channels;
    if (source->has_effects) {
        decoder_out_channels = DEFAULT_CHANNEL_NUMBER_1;
    }

    enum DecoderType decoder_type = DECODER_NONE;
    if (source->is_pcm) {
        decoder_type = DECODER_PCM;
    } else {
        decoder_type = DECODER_FFMPEG;
    }
    decoder = audio_decoder_create(source->file_path,
        source->sample_rate, source->nb_channels,
        dst_sample_rate, decoder_out_channels,
        source->volume, decoder_type);
    if (!decoder) {
        LogError("%s malloc source decoder failed.\n", __func__);
        return NULL;
    }

    if (IAudioDecoder_set_crop_pos(decoder,
        source->crop_start_time_ms, source->crop_end_time_ms) < 0) {
        LogError("%s IAudioDecoder_set_crop_pos failed.\n", __func__);
        IAudioDecoder_freep(&decoder);
        return NULL;
    }

    source->fade_io.fade_in_nb_samples = source->fade_io.fade_in_time_ms * dst_sample_rate / 1000;
    source->fade_io.fade_out_nb_samples = source->fade_io.fade_out_time_ms * dst_sample_rate / 1000;
    source->yl_prev = source->makeup_gain * MAKEUP_GAIN_MAX_DB;

    IAudioDecoder_seekTo(decoder, seek_time_ms);

    if (source->has_effects) {
        audio_effect_freep(&source->effects_ctx);
        source->effects_ctx = audio_effect_create();
        if (audio_effect_init(source->effects_ctx,
                decoder, source->effects_info, dst_channels) < 0) {
            LogError("%s audio_effect_init failed.\n", __func__);
            IAudioDecoder_freep(&decoder);
            return NULL;
        }
    }

    if (source->buffer.buffer) free(source->buffer.buffer);
    source->buffer.buffer =
        (short *)calloc(sizeof(short), MAX_NB_SAMPLES);
    if (!source->buffer.buffer) {
        LogError("%s calloc Source Buffer failed.\n", __func__);
        IAudioDecoder_freep(&decoder);
        return NULL;
    }

    source->decoder = decoder;
    return decoder;
}

static int update_audio_source(AudioSourceQueue *queue,
        AudioSource *source, int dst_sample_rate, int dst_channels) {
    int ret = -1;
    if (!queue || !source || (AudioSourceQueue_size(queue) <= 0))
        return ret;

    while (AudioSourceQueue_size(queue) > 0) {
        AudioSource_free(source);
        if (AudioSourceQueue_get(queue, source) > 0) {
            source->decoder = open_source_decoder(source,
                dst_sample_rate, dst_channels, 0);
            if (!source->decoder)
            {
                LogError("%s open decoder failed, file_path: %s.\n",
                    __func__, source->file_path);
                ret = AEERROR_NOMEM;
            } else {
                ret = 0;
                break;
            }
        }
    }
    return ret;
}

static void audio_source_seekTo(AudioSourceQueue *queue,
        AudioSource *source, int dst_sample_rate,
        int dst_channels, int seek_time_ms) {
    LogInfo("%s\n", __func__);
    if (!queue || !source || (AudioSourceQueue_size(queue) <= 0))
        return;

    int source_seek_time = 0;
    bool find_source = false;
    while ((AudioSourceQueue_size(queue) > 0)) {
        AudioSource_free(source);
        if (AudioSourceQueue_get(queue, source) > 0) {
            if (source->start_time_ms <= seek_time_ms) {
                if (source->end_time_ms <= seek_time_ms) {
                    source_seek_time = 0;
                } else {
                    source_seek_time = seek_time_ms - source->start_time_ms;
                    find_source = true;
                    break;
                }
            } else {
                source_seek_time = 0;
                find_source = true;
                break;
            }
        }
    }

    if (find_source)
        open_source_decoder(source, dst_sample_rate,
            dst_channels, source_seek_time);
    else
        AudioSource_free(source);
}

static void fade_in_out(AudioSource *source, int sample_rate,
        int channels, int pcm_start_time, int pcm_duration,
        short *dst_buffer, int dst_buffer_size) {
    if (!source || !dst_buffer)
        return;

    check_fade_in_out(&(source->fade_io), pcm_start_time, pcm_duration,
        sample_rate, source->start_time_ms, source->end_time_ms);
    scale_with_ramp(&(source->fade_io), dst_buffer,
        dst_buffer_size / channels, channels);
}

static AudioMuxer *open_muxer(int dst_sample_rate, int dst_channels,
	int bytes_per_sample, const char *out_file_path, int encoder_type) {
    LogInfo("%s\n", __func__);
    if (!out_file_path )
        return NULL;
    AudioMuxer *muxer = NULL;

    MuxerConfig config;
    config.src_sample_rate_in_Hz = dst_sample_rate;
    config.src_nb_channels = dst_channels;
    config.dst_sample_rate_in_Hz = dst_sample_rate;
    config.dst_nb_channels = dst_channels;
    config.muxer_name = MUXER_AUDIO_MP4;
    config.mime = MIME_AUDIO_AAC;
    config.codec_id = AV_CODEC_ID_AAC;
    config.output_filename = av_strdup(out_file_path);
    switch (bytes_per_sample) {
        case 1:
            config.src_sample_fmt = AV_SAMPLE_FMT_U8;
            break;
        case 2:
            config.src_sample_fmt = AV_SAMPLE_FMT_S16;
            break;
        case 4:
            config.src_sample_fmt = AV_SAMPLE_FMT_S32;
            break;
        default:
            LogError("%s bytes_per_sample %d is invalid.\n", __func__, bytes_per_sample);
            return NULL;
    }
    switch (encoder_type) {
        case ENCODER_FFMPEG:
            config.encoder_type = ENCODER_FFMPEG;
            break;
        case ENCODER_MEDIA_CODEC:
            config.encoder_type = ENCODER_MEDIA_CODEC;
            break;
        case ENCODER_IOS_HW:
            config.encoder_type = ENCODER_IOS_HW;
            break;
        default:
            LogError("%s encoder_type %d is invalid.\n", __func__, encoder_type);
            config.encoder_type = ENCODER_NONE;
            return NULL;
    }
    muxer = muxer_create(&config);
    free(config.output_filename);
    return muxer;
}

static int mixer_chk_st_l(int mix_state)
{
    if (mix_state == MIX_STATE_INITIALIZED) {
        return 0;
    }

    LogError("%s mixer state(%d) is invalid.\n", __func__, mix_state);
    LogError("%s expecting mix_state == MIX_STATE_INITIALIZED(1).\n", __func__);
    return -1;
}

static void mixer_abort_l(XmMixerContext *ctx)
{
    if(!ctx)
        return;

    pthread_mutex_lock(&ctx->mutex);
    ctx->abort = true;
    pthread_mutex_unlock(&ctx->mutex);
}

static void mixer_free_l(XmMixerContext *ctx)
{
    if(!ctx)
        return;

    xm_audio_mixer_stop(ctx);

    mixer_effects_free(&(ctx->mixer_effects));

    reverb_free(ctx);

    if (ctx->in_config_path) {
        free(ctx->in_config_path);
        ctx->in_config_path = NULL;
    }

    muxer_stop(ctx->muxer);
    muxer_freep(&(ctx->muxer));

    if (ctx->audio_fifo) {
        fifo_delete(&ctx->audio_fifo);
    }
    if (ctx->zero_buffer) {
        free(ctx->zero_buffer);
        ctx->zero_buffer = NULL;
    }

    pthread_mutex_lock(&ctx->mutex);
    ctx->abort= false;
    ctx->progress = 0;
    pthread_mutex_unlock(&ctx->mutex);
}

static void add_reverb_and_write_fifo(XmMixerContext *ctx,
        short *buffer, int buffer_len) {
    if (!ctx || !buffer || buffer_len <= 0)
        return;

    if (!ctx->reverb_ctx || !ctx->audio_fifo)
        return;

    volatile int read_len = -1;
    // send data
    send_samples(ctx->reverb_ctx, buffer, buffer_len);

    // receive data
    read_len = receive_samples(ctx->reverb_ctx, buffer, buffer_len);
    if (read_len > 0) {
        fifo_write(ctx->audio_fifo, buffer, read_len);
    }

    while(read_len > 0) {
        read_len = receive_samples(ctx->reverb_ctx, buffer, buffer_len);
        if (read_len > 0) {
            fifo_write(ctx->audio_fifo, buffer, read_len);
        }
    }
}

static short *mixer_combine(AudioSource *source,
    int mix_len, short *prev_buffer,
    short *dst_buffer, int channels) {
    if (!source || !source->buffer.buffer
        || !prev_buffer || !dst_buffer)
        return NULL;
    if (mix_len <= 0) {
        LogError("%s mix_len is %d.\n", __func__, mix_len);
        return NULL;
    }

    MixBufferS16(prev_buffer, source->buffer.buffer,
        mix_len / channels, channels, dst_buffer,
        &(source->left_factor), &(source->right_factor));

    return dst_buffer;
}

static int fill_track_buffer(AudioSource *source,
    int fill_len, int start_time, int duration,
    int sample_rate, int channels) {
    int ret = -1;
    if (!source || !source->buffer.buffer)
        return ret;

    short *buffer = source->buffer.buffer;
    int buffer_size_in_short = 0;
    int buffer_data_start_index = 0;
    if (start_time >= source->start_time_ms &&
        start_time + duration < source->end_time_ms) {
        memset(buffer, 0, sizeof(short) * MAX_NB_SAMPLES);
        buffer_data_start_index = 0;
        buffer_size_in_short = fill_len > 0 ? fill_len : 0;

        buffer_size_in_short = get_pcm_from_decoder(source,
            buffer, buffer_size_in_short);
        if (buffer_size_in_short <= 0) {
            goto end;
        }
    } else if (start_time < source->start_time_ms &&
            start_time + duration > source->start_time_ms) {
        memset(buffer, 0, sizeof(short) * MAX_NB_SAMPLES);
        buffer_data_start_index = ((source->start_time_ms - start_time)
            * sample_rate * channels) / 1000;
        buffer_size_in_short =
            fill_len - buffer_data_start_index > 0 ?
            fill_len - buffer_data_start_index : 0;

        buffer_size_in_short = get_pcm_from_decoder(source,
            buffer + buffer_data_start_index, buffer_size_in_short);
        if (buffer_size_in_short <= 0) {
            goto end;
        }
    } else if (start_time < source->end_time_ms &&
            start_time + duration > source->end_time_ms) {
        memset(buffer, 0, sizeof(short) * MAX_NB_SAMPLES);
        buffer_data_start_index = 0;
        buffer_size_in_short = ((source->end_time_ms - start_time)
            * sample_rate * channels) / 1000;
        buffer_size_in_short =
            buffer_size_in_short > 0 ? buffer_size_in_short : 0;

        buffer_size_in_short = get_pcm_from_decoder(source,
            buffer, buffer_size_in_short);
        if (buffer_size_in_short <= 0) {
            goto end;
        }
    } else if(start_time >= source->end_time_ms) {
        //update the decoder that point the next bgm
        AudioSource_free(source);
        goto end;
    } else {
        goto end;
    }

    fade_in_out(source, sample_rate, channels, start_time,
        duration, buffer + buffer_data_start_index, buffer_size_in_short);

    ret = 0;
end:
    return ret;
}

static int mixer_mix_and_write_fifo(XmMixerContext *ctx) {
    if (!ctx) return -1;
    int ret = -1, read_len = MAX_NB_SAMPLES;
    int buffer_start_ms = ctx->seek_time_ms +
        calculation_duration_ms(ctx->cur_size,
        ctx->bits_per_sample >> 3, ctx->dst_channels,
        ctx->dst_sample_rate);
    int duration = calculation_duration_ms(
        read_len * sizeof(short), ctx->bits_per_sample >> 3,
        ctx->dst_channels, ctx->dst_sample_rate);

    int file_duration = ctx->mixer_effects.duration_ms;
    if (buffer_start_ms >= file_duration) {
        ret = PCM_FILE_EOF;
        goto end;
    } else if ((buffer_start_ms + duration >= file_duration)) {
        duration = file_duration - buffer_start_ms;
        read_len = (duration * ctx->dst_sample_rate
            * ctx->dst_channels) / 1000;
    }

    short *prev_buffer = NULL;
    for (int i = 0; i < MAX_NB_TRACKS; i++) {
        AudioSource *source = ctx->mixer_effects.source[i];
        AudioSourceQueue *queue = ctx->mixer_effects.sourceQueue[i];
        if (!source->decoder && AudioSourceQueue_size(queue) > 0) {
            update_audio_source(queue, source,
                ctx->dst_sample_rate, ctx->dst_channels);
        }

        if (source->decoder) {
            if (fill_track_buffer(source, read_len, buffer_start_ms,
                duration, ctx->dst_sample_rate, ctx->dst_channels) < 0) {
                    source->buffer.mute = true;
                } else {
                    source->buffer.mute = false;
                }
        } else {
            source->buffer.mute = true;
        }

        if (!source->buffer.mute) {
            if (prev_buffer != NULL) {
                if (source->side_chain_enable) {
                    side_chain_compress(prev_buffer,
                        source->buffer.buffer, &(source->yl_prev),
                        read_len, ctx->dst_sample_rate, ctx->dst_channels,
                        SIDE_CHAIN_THRESHOLD, SIDE_CHAIN_RATIO,
                        SIDE_CHAIN_ATTACK_MS, SIDE_CHAIN_RELEASE_MS,
                        source->makeup_gain);
                }
                prev_buffer = mixer_combine(source, read_len,
                    prev_buffer, source->buffer.buffer,
                    ctx->dst_channels);
            } else {
                if (source->side_chain_enable) {
                    side_chain_compress(ctx->zero_buffer,
                        source->buffer.buffer, &(source->yl_prev),
                        read_len, ctx->dst_sample_rate, ctx->dst_channels,
                        SIDE_CHAIN_THRESHOLD, SIDE_CHAIN_RATIO,
                        SIDE_CHAIN_ATTACK_MS, SIDE_CHAIN_RELEASE_MS,
                        source->makeup_gain);
                }
                prev_buffer = source->buffer.buffer;
            }
        }
    }

    ctx->cur_size += (read_len * sizeof(short));
    if (!prev_buffer) {
        fifo_write(ctx->audio_fifo, ctx->zero_buffer, read_len);
        return read_len;
    }

    add_reverb_and_write_fifo(ctx, prev_buffer, read_len);
    ret = read_len;

end:
    return ret;
}

void xm_audio_mixer_freep(XmMixerContext **ctx) {
    LogInfo("%s\n", __func__);
    if (NULL == ctx || NULL == *ctx)
        return;
    XmMixerContext *self = *ctx;

    mixer_free_l(self);
    pthread_mutex_destroy(&(self->mutex));
    free(*ctx);
    *ctx = NULL;
}

void xm_audio_mixer_stop(XmMixerContext *ctx) {
    LogInfo("%s\n", __func__);
    if (NULL == ctx)
        return;

    mixer_abort_l(ctx);
}

int xm_audio_mixer_get_progress(XmMixerContext *ctx) {
    if (NULL == ctx)
        return 0;

    int ret = 0;
    pthread_mutex_lock(&ctx->mutex);
    ret = ctx->progress;
    pthread_mutex_unlock(&ctx->mutex);

    return ret;
}

int xm_audio_mixer_get_frame(XmMixerContext *ctx,
    short *buffer, int buffer_size_in_short) {
    int ret = -1;
    if (!ctx || !buffer || buffer_size_in_short < 0)
        return ret;

    while (fifo_occupancy(ctx->audio_fifo) < (size_t) buffer_size_in_short) {
	ret = mixer_mix_and_write_fifo(ctx);
	if (ret < 0) {
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

int xm_audio_mixer_seekTo(XmMixerContext *ctx,
        int seek_time_ms) {
    LogInfo("%s seek_time_ms %d.\n", __func__, seek_time_ms);
    if (!ctx)
        return -1;

    ctx->seek_time_ms = seek_time_ms > 0 ? seek_time_ms : 0;
    if (ctx->audio_fifo) fifo_clear(ctx->audio_fifo);
    ctx->cur_size = 0;

    for (int i = 0; i < MAX_NB_TRACKS; i++) {
        AudioSourceQueue_copy(
            ctx->mixer_effects.sourceQueueBackup[i],
            ctx->mixer_effects.sourceQueue[i]);
        audio_source_seekTo(ctx->mixer_effects.sourceQueue[i],
            ctx->mixer_effects.source[i], ctx->dst_sample_rate,
            ctx->dst_channels, ctx->seek_time_ms);
    }
    return 0;
}

static int xm_audio_mixer_mix_l(XmMixerContext *ctx,
    int encoder_type, const char *out_file_path) {
    LogInfo("%s.\n", __func__);
    int ret = -1;
    short *buffer = NULL;
    if (!ctx || !out_file_path) {
        return ret;
    }

    ctx->muxer = open_muxer(ctx->dst_sample_rate, ctx->dst_channels,
        ctx->bits_per_sample >> 3, out_file_path, encoder_type);
    if (!ctx->muxer)
    {
        LogError("%s open_muxer failed.\n", __func__);
        ret = AEERROR_NOMEM;
        goto fail;
    }

    buffer = (short *)calloc(sizeof(short), MAX_NB_SAMPLES);
    if (!buffer) {
        LogError("%s calloc buffer failed.\n", __func__);
        ret = AEERROR_NOMEM;
        goto fail;
    }

    ctx->seek_time_ms = 0;
    ctx->cur_size = 0;
    int file_duration = ctx->mixer_effects.duration_ms;
    //if (file_duration > MAX_DURATION_MIX_IN_MS) file_duration = MAX_DURATION_MIX_IN_MS;
    while (!ctx->abort) {
        int cur_position = ctx->seek_time_ms +
            calculation_duration_ms(ctx->cur_size, ctx->bits_per_sample >> 3,
            ctx->dst_channels, ctx->dst_sample_rate);
        int progress = ((float)cur_position / file_duration) * 100;
        pthread_mutex_lock(&ctx->mutex);
        ctx->progress = progress;
        pthread_mutex_unlock(&ctx->mutex);

        ret = xm_audio_mixer_get_frame(ctx, buffer, MAX_NB_SAMPLES);
        if (ret <= 0) {
            LogInfo("xm_audio_mixer_get_frame len <= 0.\n");
            break;
        }

        ret = muxer_write_audio_frame(ctx->muxer, buffer, ret);
        if (ret < 0) {
            LogError("muxer_write_audio_frame failed\n");
            goto fail;
        }
    }

    if (PCM_FILE_EOF == ret) ret = 0;
fail:
    if (buffer != NULL) {
        free(buffer);
        buffer = NULL;
    }
    muxer_stop(ctx->muxer);
    muxer_freep(&(ctx->muxer));
    return ret;
}

int xm_audio_mixer_mix(XmMixerContext *ctx,
    const char *out_file_path, int encoder_type)
{
    LogInfo("%s out_file_path = %s, encoder_type = %d.\n", __func__, out_file_path, encoder_type);
    int ret = -1;
    if (NULL == ctx || NULL == out_file_path) {
        return ret;
    }

    if (mixer_chk_st_l(ctx->mix_status) < 0) {
        return AEERROR_INVALID_STATE;
    }

    pthread_mutex_lock(&ctx->mutex);
    ctx->mix_status = MIX_STATE_STARTED;
    pthread_mutex_unlock(&ctx->mutex);

    if ((ret = xm_audio_mixer_mix_l(ctx, encoder_type, out_file_path)) < 0) {
        LogError("%s mixer_audio_mix_l failed\n", __func__);
        goto fail;
    }

    pthread_mutex_lock(&ctx->mutex);
    ctx->mix_status = MIX_STATE_COMPLETED;
    pthread_mutex_unlock(&ctx->mutex);
    return ret;
fail:
    mixer_free_l(ctx);
    pthread_mutex_lock(&ctx->mutex);
    ctx->mix_status = MIX_STATE_ERROR;
    pthread_mutex_unlock(&ctx->mutex);
    return ret;
}

int xm_audio_mixer_init(XmMixerContext *ctx,
        const char *in_config_path)
{
    int ret = -1;
    if (!ctx || !in_config_path) {
        return ret;
    }
    LogInfo("%s in_config_path = %s.\n", __func__, in_config_path);

    mixer_free_l(ctx);
    ctx->dst_sample_rate = DEFAULT_SAMPLE_RATE;
    ctx->dst_channels = DEFAULT_CHANNEL_NUMBER_2;
    ctx->bits_per_sample = BITS_PER_SAMPLE_16;
    ctx->cur_size = 0;
    ctx->seek_time_ms = 0;
    ctx->in_config_path = av_strdup(in_config_path);

    if ((ret = mixer_effects_init(&(ctx->mixer_effects))) < 0) {
        LogError("%s mixer_effects_init failed\n", __func__);
        goto fail;
    }

    if ((ret = json_parse(&(ctx->mixer_effects), in_config_path)) < 0) {
        LogError("%s json_parse %s failed\n", __func__, in_config_path);
        goto fail;
    }

    // Allocate buffer for audio fifo
    ctx->audio_fifo = fifo_create(sizeof(short));
    if (!ctx->audio_fifo) {
        LogError("%s Could not allocate audio FIFO\n", __func__);
        ret = AEERROR_NOMEM;
        goto fail;
    }

    ctx->zero_buffer = (short *)calloc(sizeof(short), MAX_NB_SAMPLES);
    if (!ctx->zero_buffer) {
        LogError("%s calloc zero_buffer failed.\n", __func__);
        ret = AEERROR_NOMEM;
        goto fail;
    }

    if ((ret = reverb_init(ctx)) < 0) {
        LogError("%s reverb_init failed\n", __func__);
        goto fail;
    }

    pthread_mutex_lock(&ctx->mutex);
    ctx->mix_status = MIX_STATE_INITIALIZED;
    pthread_mutex_unlock(&ctx->mutex);

    return ret;
fail:
    mixer_free_l(ctx);
    pthread_mutex_lock(&ctx->mutex);
    ctx->mix_status = MIX_STATE_ERROR;
    pthread_mutex_unlock(&ctx->mutex);
    return ret;
}

XmMixerContext *xm_audio_mixer_create()
{
    LogInfo("%s.\n", __func__);
    XmMixerContext *self = (XmMixerContext *)calloc(1, sizeof(XmMixerContext));
    if (!self) {
        LogError("%s alloc XmMixerContext failed.\n", __func__);
        return NULL;
    }

    self->dst_sample_rate = DEFAULT_SAMPLE_RATE;
    self->dst_channels = DEFAULT_CHANNEL_NUMBER_2;
    self->bits_per_sample = BITS_PER_SAMPLE_16;
    pthread_mutex_init(&self->mutex, NULL);
    self->mix_status = MIX_STATE_UNINIT;

    return self;
}
