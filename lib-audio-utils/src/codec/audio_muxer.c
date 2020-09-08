#if defined(__ANDROID__) || defined (__linux__)
#include "audio_muxer.h"
#include "log.h"
#include "error_def.h"
#include "sw/audio_encoder_sw.h"
#if defined(__ANDROID__)
#include "mediacodec/audio_encoder_mediacodec.h"
#include "xm_android_jni.h"
#include <jni.h>
#endif
#if defined(__APPLE__)
#include "hw/audio_encoder_ios_hw.h"
#endif

#define MONO_BIT_RATE 64000
#define STEREO_BIT_RATE 128000
#define AUDIO_FIFO_MAX_SIZE_IN_FRAME 10

static int read_encode_and_save(AudioMuxer *am);

static void release(AudioMuxer *am) {
    LogInfo("%s.\n", __func__);
    if(!am)
        return;

    if (am->copy_buffer) {
        av_freep(&(am->copy_buffer[0]));
        av_freep(&(am->copy_buffer));
    }
    if (am->dst_data) {
        av_freep(&(am->dst_data[0]));
        av_freep(&(am->dst_data));
    }
    if (am->swr_ctx) {
        swr_free(&am->swr_ctx);
        am->swr_ctx = NULL;
    }
    if (am->enc_ctx) {
        avcodec_free_context(&am->enc_ctx);
        am->enc_ctx = NULL;
    }
    if (am->encode_fifo) {
        av_audio_fifo_free(am->encode_fifo);
        am->encode_fifo = NULL;
    }
    if (am->audio_frame) {
        av_frame_free(&am->audio_frame);
        am->audio_frame = NULL;
    }
    if (am->ofmt_ctx) {
        if (!(am->ofmt_ctx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&(am->ofmt_ctx->pb));
        avformat_free_context(am->ofmt_ctx);
        am->ofmt_ctx = NULL;
    }
    if (am->audio_encoder) {
        ff_encoder_free_p(&(am->audio_encoder));
    }

    if(am->config.mime)
        av_free(am->config.mime);
    if(am->config.muxer_name)
        av_free(am->config.muxer_name);
    if(am->config.output_filename)
        av_free(am->config.output_filename);
    memset(&(am->config), 0, sizeof(MuxerConfig));
}

static int fifo_size(AudioMuxer *am)
{
    int ret = -1;
    if(!am || !am->encode_fifo)
        return ret;

    pthread_mutex_lock(&am->mutex);
    ret = av_audio_fifo_size(am->encode_fifo);
    pthread_mutex_unlock(&am->mutex);
    return ret;
}

static int fifo_put(AudioMuxer *am, int nb_samples, void** buffer)
{
    int ret = -1;
    if(!am || !am->encode_fifo
        || !buffer || !(*buffer) || nb_samples < 0)
        return ret;

    pthread_mutex_lock(&am->mutex);
    ret = AudioFifoPut(am->encode_fifo, nb_samples, buffer);
    pthread_mutex_unlock(&am->mutex);
    return ret;
}

static int fifo_get(AudioMuxer *am, int nb_samples, void** buffer)
{
    int ret = -1;
    if(!am || !am->encode_fifo
        || !buffer || !(*buffer) || nb_samples < 0)
        return ret;

    pthread_mutex_lock(&am->mutex);
    ret = AudioFifoGet(am->encode_fifo, nb_samples, buffer);
    pthread_mutex_unlock(&am->mutex);
    return ret;
}

static bool is_abort(AudioMuxer *am)
{
    if(!am)
        return true;

    bool ret = false;
    pthread_mutex_lock(&am->mutex);
    ret = am->abort;
    pthread_mutex_unlock(&am->mutex);
    return ret;
}

static void notify(AudioMuxer *am)
{
    if(!am)
        return;

    pthread_mutex_lock(&am->mutex);
    pthread_cond_signal(&am->condition);
    pthread_mutex_unlock(&am->mutex);
}

static void wait_on_notify(AudioMuxer *am)
{
    if(!am)
        return;

    pthread_mutex_lock(&am->mutex);
    if (!am->abort) {
        pthread_cond_wait(&am->condition, &am->mutex);
    }
    pthread_mutex_unlock(&am->mutex);
}

static void thread_abort(AudioMuxer *am)
{
    if(!am)
        return;

    pthread_mutex_lock(&am->mutex);
    am->abort = true;
    pthread_cond_signal(&am->condition);
    pthread_mutex_unlock(&am->mutex);
}

static void thread_join(AudioMuxer *am)
{
    if(!am || !am->running)
        return;

    SDL_WaitThread(am->mux_thread, NULL);
}

static int write_audio_packet(AudioMuxer *am, AVPacket *pkt)
{
    int ret = 0;
    AVStream *out_stream = am->ofmt_ctx->streams[am->audio_stream_index];
    AVRational time_base = am->enc_ctx->time_base;

    pkt->stream_index = am->audio_stream_index;
    av_packet_rescale_ts(pkt, time_base, out_stream->time_base);
    pkt->pos = -1;

    if((ret = av_interleaved_write_frame(am->ofmt_ctx, pkt)) < 0) {
        LogError("%s av_interleaved_write_frame failed.\n", __func__);
    }

    return ret;
}

static bool encoder_flush(AudioMuxer *am)
{
    int ret = 0;
    int got_packet = 0;
    int i = 0;

    while (fifo_size(am) > 0) {
        if ((ret = read_encode_and_save(am)) < 0)
        {
            LogError("%s read_encode_and_save error, ret = %d.\n",
                __func__, ret);
            ret = -1;
            goto end;
        }
    }

    for (got_packet = 1; got_packet; i++)
    {
        AVPacket packet;
        InitPacket(&packet);
        ret = ff_encoder_encode_frame(am->audio_encoder, NULL, &packet,
            &got_packet);
        if (ret < 0) {
            LogError("Could not encode frame (error '%s'), error code = %d.\n",
                av_err2str(ret), ret);
            av_packet_unref(&packet);
            goto end;
        }

        if (got_packet) {
            if ((ret = write_audio_packet(am, &packet)) < 0) {
                LogError("%s write_audio_packet ERROR & return.\n", __func__);
                av_packet_unref(&packet);
                goto end;
            }
        }
        av_packet_unref(&packet);
    }

end:
    return ret >= 0 ? true : false;
}

static int encode_and_save(AudioMuxer *am, AVFrame *audio_frame) {
    if (!am || !audio_frame)
        return kNullPointError;
    int ret = 0;
    int got_packet = 0;
    AVPacket packet;

    InitPacket(&packet);
    audio_frame->pts = am->audio_encode_pts;
    am->audio_encode_pts += am->frame_size;
    ret = ff_encoder_encode_frame(am->audio_encoder, audio_frame, &packet,
            &got_packet);
    if (ret < 0) {
        LogError("Could not encode frame (error '%s'), error code = %d.\n", av_err2str(ret), ret);
        goto end;
    }

    if (got_packet) {
        if ((ret = write_audio_packet(am, &packet)) < 0) {
            LogError("%s write_audio_packet ERROR & return.\n", __func__);
            goto end;
        }
    }

end:
    av_packet_unref(&packet);
    if (NULL == audio_frame) ret = AVERROR_EXIT;
    return ret;
}

static int read_encode_and_save(AudioMuxer *am) {
    if (!am)
        return kNullPointError;
    int ret = 0;

    ret = fifo_get(am, am->frame_size, (void**)(am->audio_frame->data));
    if (ret < 0) {
        LogError("%s AudioFifoGet failed.\n", __func__);
        goto end;
    }

    ret = encode_and_save(am, am->audio_frame);
end:
    return ret < 0 ? ret : 0;
}

static int resample_audio(AudioMuxer *am, const uint8_t **src_data,
        int src_nb_samples) {
    if (!am || !src_data || !*src_data)
        return kNullPointError;

    int ret = 0;
    int dst_nb_samples = 0;
    if (am->swr_ctx) {
        dst_nb_samples = swr_get_out_samples(am->swr_ctx, src_nb_samples);
        if (dst_nb_samples > am->max_dst_nb_samples) {
            ret = AllocateSampleBuffer(&(am->dst_data), am->config.dst_nb_channels,
                dst_nb_samples, AV_SAMPLE_FMT_S16);
            if (ret < 0) {
                LogError("%s av_samples_alloc error!\n", __func__);
                goto end;
            }
            am->max_dst_nb_samples = dst_nb_samples;
        }

        ret = dst_nb_samples = swr_convert(am->swr_ctx, am->dst_data,
            dst_nb_samples, src_data, src_nb_samples);
        if (ret < 0) {
            LogError("%s swr_convert error!\n", __func__);
            goto end;
        }
    }

end:
    return ret;
}

static int add_samples_to_encode_fifo(AudioMuxer *am,
        uint8_t **src_data, int src_nb_samples) {
    if (!am || !am->encode_fifo || !src_data)
        return kNullPointError;

    int ret = 0;
    if (am->swr_ctx) {
        ret = resample_audio(am, (const uint8_t **)src_data, src_nb_samples);
        if (ret < 0) {
            LogError("%s resample_audio failed.\n", __func__);
            goto end;
        }

        ret = fifo_put(am, ret, (void **)am->dst_data);
        if (ret < 0) {
            LogError("%s swr_ctx: AudioFifoPut failed.\n", __func__);
            goto end;
        }
    } else {
        ret = fifo_put(am, src_nb_samples, (void **)src_data);
        if (ret < 0) {
            LogError("%s AudioFifoPut failed.\n", __func__);
            goto end;
        }
    }

    notify(am);
end:
    return ret;
}

static int copy_audio_buffer(AudioMuxer *am, const short *buffer,
        int buffer_size_in_short) {
    if (!am || !am->copy_buffer || !buffer)
        return kNullPointError;

    int ret = 0;
    int nb_samples = buffer_size_in_short / am->config.src_nb_channels;
    if (nb_samples > am->max_nb_copy_samples) {
        am->max_nb_copy_samples = nb_samples;
        ret = AllocateSampleBuffer(&(am->copy_buffer), am->config.src_nb_channels,
            am->max_nb_copy_samples, am->config.src_sample_fmt);
        if (ret < 0) {
            LogError("%s ReAllocateSampleCopyBuffer failed.\n", __func__);
            goto end;
        }
    }
    memcpy(am->copy_buffer[0], buffer, sizeof(short) * (size_t)buffer_size_in_short);

end:
    return ret;
}

static int create_audio_encoder(AudioMuxer *am) {
    if (!am)
        return kNullPointError;
    int ret = 0;
    AVDictionary *audio_opt = NULL;

    if (am->config.encoder_type == ENCODER_FFMPEG) {
        am->audio_encoder = ff_encoder_sw_create();
        if (NULL == am->audio_encoder) {
            LogError("%s ff_encoder_sw_create failed.\n", __func__);
            ret = -1;
            goto end;
        }
#if defined(__ANDROID__)
    } else if (am->config.encoder_type == ENCODER_MEDIA_CODEC) {
        am->audio_encoder = ff_encoder_mediacodec_create();
        if (NULL == am->audio_encoder) {
            LogError("%s ff_encoder_mediacodec_create failed.\n", __func__);
            ret = -1;
            goto end;
        }
#endif
#if defined(__APPLE__)
    } else if (am->config.encoder_type == ENCODER_IOS_HW) {
        am->audio_encoder = ff_encoder_ios_hw_create();
        if (NULL == am->audio_encoder) {
            LogError("%s ff_encoder_ios_hw_create failed.\n", __func__);
            ret = -1;
            goto end;
        }
#endif
    } else {
        LogError("%s unsupport encoder_type %d.\n", __func__, am->config.encoder_type);
        ret = -1;
        goto end;
    }

    av_dict_set(&audio_opt, "mime", am->config.mime, 0);
    av_dict_set_int(&audio_opt, "bit_rate", am->config.dst_bit_rate, 0);
    av_dict_set_int(&audio_opt, "sample_rate", am->config.dst_sample_rate_in_Hz, 0);
    av_dict_set_int(&audio_opt, "channels", am->config.dst_nb_channels, 0);
    av_dict_set_int(&audio_opt, "channel_layout",
    av_get_default_channel_layout(am->config.dst_nb_channels), 0);

    ret = ff_encoder_config(am->audio_encoder, audio_opt);
    if (ret < 0) goto end;

    am->frame_size =
            ff_encoder_get_frame_size(am->audio_encoder) / am->config.dst_nb_channels
            / av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
    if (am->frame_size != am->enc_ctx->frame_size) {
        LogError("%s audio encoder frame_size != am->enc_ctx->frame_size.\n", __func__);
        ret = -1;
        goto end;
    }

end:
    if (audio_opt) av_dict_free(&audio_opt);
    return ret;
}

static int open_output_file(AudioMuxer *am)
{
    if (!am)
        return kNullPointError;

    int ret = 0;
    MuxerConfig *config = &am->config;
    AVCodecContext *avctx = NULL;
    if ((ret = avformat_alloc_output_context2(&am->ofmt_ctx, NULL,
        config->muxer_name, config->output_filename)) < 0)
    {
        LogError("%s avformat_alloc_output_context2 failed, file addr: %s.\n", __func__, config->output_filename);
        goto end;
    }

    if((ret = FindAndOpenAudioEncoder(&avctx, config->codec_id,
        config->dst_bit_rate, config->dst_nb_channels,
        config->dst_sample_rate_in_Hz)) < 0)
    {
        LogError("%s FindAndOpenAudioEncoder failed!errCode : %s.\n", __func__, av_err2str(ret));
        goto end;
    }
    am->enc_ctx = avctx;

    if ((ret = AddAudioStream(am->ofmt_ctx, avctx)) < 0) {
        LogError("%s AddAudioStream failed!errCode : %s.\n", __func__, av_err2str(ret));
        goto end;
    }
    am->audio_stream_index = ret;
    av_opt_set(am->ofmt_ctx, "movflags", "faststart", AV_OPT_SEARCH_CHILDREN);

    if(!(am->ofmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        if((ret = avio_open(&am->ofmt_ctx->pb, config->output_filename,
            AVIO_FLAG_WRITE)) < 0)
        {
            LogError("%s avio_open failed!output_filename : %s.\n", __func__, config->output_filename);
            goto end;
        }
    }

    return ret;
end:
    release(am);
    return ret;
}

static int write_frame(AudioMuxer *am) {
    int ret = -1;
    if (!am || !am->encode_fifo || !am->enc_ctx) {
        LogError("%s kNullPointError.\n", __func__);
        return kNullPointError;
    }

    while (!is_abort(am)) {
        if (fifo_size(am) >= am->frame_size) {
            ret = read_encode_and_save(am);
            if (ret < 0) {
                LogError("%s read_encode_and_save failed.\n", __func__);
                goto end;
            }
        }

        if (fifo_size(am) < AUDIO_FIFO_MAX_SIZE_IN_FRAME * am->frame_size) {
            notify(am);
        }

        if (fifo_size(am) < am->frame_size) {
            wait_on_notify(am);
        }
    }

end:
    return ret;
}

static int write_output_file_header(AVFormatContext *ofmt_ctx) {
    int ret = 0;

    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        LogError("%s avformat_write_header (error '%s').\n", __func__, av_err2str(ret));
    }
    return ret;
}

static int init_encoder_muxer(AudioMuxer *am, MuxerConfig *config)
{
    LogInfo("%s.\n", __func__);
    if (!am || !config)
        return kNullPointError;
    int ret = -1;

    release(am);
    am->config = *config;
    am->config.mime = av_strdup(config->mime);
    am->config.muxer_name = av_strdup(config->muxer_name);
    am->config.output_filename = av_strdup(config->output_filename);
    am->config.dst_bit_rate = config->dst_nb_channels >= 2 ? STEREO_BIT_RATE : MONO_BIT_RATE;
    am->max_nb_copy_samples = MAX_NB_SAMPLES;
    am->max_dst_nb_samples = MAX_NB_SAMPLES;
    am->audio_stream_index = -1;
    am->audio_encode_pts = 0;

    ret = CheckSampleRateAndChannels(config->dst_sample_rate_in_Hz,
        config->dst_nb_channels);
    if (ret < 0) {
        LogError("%s CheckSampleRateAndChannels failed.\n", __func__);
        goto end;
    }

    if ((ret = open_output_file(am)) < 0) {
        LogError("%s open_output_file failed.\n", __func__);
        goto end;
    }

    if ((ret = create_audio_encoder(am)) < 0) {
        LogError("%s create_config_encoder failed.\n", __func__);
        goto end;
    }

    // Allocate sample buffer for copy_buffer
    ret = AllocateSampleBuffer(&(am->copy_buffer), config->src_nb_channels,
        am->max_nb_copy_samples, config->src_sample_fmt);
    if (ret < 0) {
        LogError("%s Allocate sample copy buffer failed.\n", __func__);
        goto end;
    }

    ret = InitResampler(config->src_nb_channels, config->dst_nb_channels,
        config->src_sample_rate_in_Hz, config->dst_sample_rate_in_Hz,
        config->src_sample_fmt, AV_SAMPLE_FMT_S16, &(am->swr_ctx));
    if (ret < 0) {
        LogError("%s InitResampler failed.\n", __func__);
        goto end;
    }

    ret = AllocateSampleBuffer(&(am->dst_data), config->dst_nb_channels,
        am->max_dst_nb_samples, AV_SAMPLE_FMT_S16);
    if (ret < 0) {
        LogError("%s  Allocate sample dst buffer failed.\n", __func__);
        goto end;
    }

    ret = AllocAudioFifo(AV_SAMPLE_FMT_S16, config->dst_nb_channels,
        &(am->encode_fifo));
    if (ret < 0) {
        LogError("%s  Allocate audio fifo failed.\n", __func__);
        goto end;
    }

    ret = AllocEncodeAudioFrame(&(am->audio_frame), config->dst_nb_channels,
        config->dst_sample_rate_in_Hz, am->frame_size,
        AV_SAMPLE_FMT_S16);
    if (ret < 0) {
        LogError("%s  Allocate audio encode frame failed.\n", __func__);
        goto end;
    }

    return ret;
end:
    release(am);
    return ret;
}

static int thread_startup(void *arg)
{
    LogInfo("mux thread start up.\n");
    if (!arg) {
        LogError("%s arg is NULL.\n", __func__);
        return kNullPointError;
    }

#if defined(__ANDROID__)
    JNIEnv *env = NULL;
    if (JNI_OK != SDL_JNI_SetupThreadEnv(&env)) {
        LogError("%s SetupThreadEnv failed.\n", __func__);
        return -1;
    }
#endif

    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);

    int ret = -1;
    AudioMuxer *am = (AudioMuxer *)arg;
    pthread_mutex_lock(&am->mutex);
    am->abort = false;
    am->running = true;
    pthread_mutex_unlock(&am->mutex);

    if((ret = write_frame(am)) < 0) {
        LogError("%s write_frame failed.\n", __func__);
        goto end;
    }

    if(!encoder_flush(am)) {
        LogWarning("%s encoder_flush failed.\n", __func__);
    }

    if((ret = av_write_trailer(am->ofmt_ctx)) < 0)
    {
        LogError("%s error occurred when av_write_trailer.\n", __func__);
        goto end;
    }

end:
    if(am->enc_ctx)
        avcodec_free_context(&(am->enc_ctx));
    am->enc_ctx = NULL;

    if (am->ofmt_ctx && !(am->ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&(am->ofmt_ctx->pb));
    avformat_free_context(am->ofmt_ctx);
    am->ofmt_ctx = NULL;

    pthread_mutex_lock(&am->mutex);
    am->running = false;
    pthread_mutex_unlock(&am->mutex);
    LogInfo("mux thread exit.\n");
    return ret;
}

static int start_async(AudioMuxer *am) {
    if (!am) {
        LogError("%s kNullPointError.\n", __func__);
        return kNullPointError;
    }

    am->mux_thread = SDL_CreateThreadEx(&am->_mux_thread, thread_startup,
        am, "audio muxer thread");
    if (!am->mux_thread) {
        LogError("%s mux_thread SDL_CreateThread() failed\n", __func__);
        return -1;
    }

    return 0;
}

void muxer_free(AudioMuxer *am)
{
    LogInfo("%s.\n", __func__);
    if (!am)
        return;

    release(am);
    pthread_mutex_destroy(&am->mutex);
    pthread_cond_destroy(&am->condition);
}

void muxer_freep(AudioMuxer **am)
{
    LogInfo("%s.\n", __func__);
    if (!am || !*am)
        return;

    muxer_stop(*am);
    muxer_free(*am);
    free(*am);
    *am = NULL;
}

int muxer_write_audio_frame(AudioMuxer *am, const short *buffer,
        int buffer_size_in_short) {
    if (!am || is_abort(am)) return 0;

    if (!am->copy_buffer || !am->encode_fifo || !am->enc_ctx) {
        LogError("%s kNullPointError.\n", __func__);
        return kNullPointError;
    }

    int ret = copy_audio_buffer(am, buffer, buffer_size_in_short);
    if (ret < 0) {
        LogError("%s copy_audio_buffer failed.\n", __func__);
        goto end;
    }

    ret = add_samples_to_encode_fifo(am, am->copy_buffer,
        buffer_size_in_short / am->config.src_nb_channels);
    if (ret < 0) {
        LogError("%s add_samples_to_encode_fifo failed.\n", __func__);
        goto end;
    }

    if (fifo_size(am) >= AUDIO_FIFO_MAX_SIZE_IN_FRAME * am->frame_size) {
        wait_on_notify(am);
    }

end:
    return ret;
}

void muxer_stop(AudioMuxer *am)
{
    if(!am)
        return;

    thread_abort(am);
    thread_join(am);
    LogInfo("%s end.\n", __func__);
}

AudioMuxer *muxer_create(MuxerConfig *config)
{
    LogInfo("%s.\n", __func__);
    if (!config)
        return NULL;
    int ret = -1;

    AudioMuxer *muxer = (AudioMuxer *)calloc(1, sizeof(AudioMuxer));
    if (NULL == muxer) {
        LogError("%s Could not allocate AudioMuxer.\n", __func__);
        goto end;
    }

    pthread_mutex_init(&muxer->mutex, NULL);
    pthread_cond_init(&muxer->condition, NULL);

    if ((ret = init_encoder_muxer(muxer, config)) < 0) {
        LogError("%s init muxer failed.\n", __func__);
        goto end;
    }

    if ((ret = write_output_file_header(muxer->ofmt_ctx)) < 0) {
        LogError("%s write_output_file_header failed.\n", __func__);
        goto end;
    }

    if ((ret = start_async(muxer)) < 0) {
        LogError("%s start_async failed.\n", __func__);
        goto end;
    }

    return muxer;
end:
    if (muxer) {
        muxer_freep(&muxer);
    }
    return NULL;
}
#endif
