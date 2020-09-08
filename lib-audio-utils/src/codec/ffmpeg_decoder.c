#if defined(__ANDROID__) || defined (__linux__)
#include "ffmpeg_decoder.h"
#include "log.h"
#include "error_def.h"
#include "ffmpeg_utils.h"

#define milliseconds_to_fftime(ms) (av_rescale(ms, AV_TIME_BASE, 1000))
#define fftime_to_milliseconds(ts) (av_rescale(ts, 1000, AV_TIME_BASE))
#define OUT_SAMPLE_FMT AV_SAMPLE_FMT_S16

typedef struct IAudioDecoder_Opaque {
    // seek parameters
    int seek_pos_ms;
    bool seek_req;
    int duration_ms;
    bool decode_completed;

    // play-out volume.
    short volume_fix;
    float volume_flp;

    // Output parameters
    int crop_start_time_in_ms;
    int crop_end_time_in_ms;
    int dst_sample_rate_in_Hz;
    int dst_nb_channels;
    int dst_bits_per_sample;

    AVAudioFifo* audio_fifo;
    uint8_t** copy_buffer;
    int max_nb_copy_samples;

    // Codec parameters
    AVFormatContext* fmt_ctx;
    AVCodecContext* dec_ctx;
    AVFrame* audio_frame;
    int audio_stream_index;
    volatile bool flush;

    // Resample parameters
    struct SwrContext* swr_ctx;
    int dst_nb_samples;
    int max_dst_nb_samples;
    uint8_t** dst_data;

    char* file_addr;
} IAudioDecoder_Opaque;

static void FFmpegDecoder_free(IAudioDecoder_Opaque *decoder);
static int FFmpegDecoder_set_crop_pos(
    IAudioDecoder_Opaque *decoder, int crop_start_time_ms,
    int crop_end_time_ms);

static inline void free_input_media_context(AVFormatContext **fmt_ctx,
                                         AVCodecContext **dec_ctx) {
    LogInfo("%s.\n", __func__);
    if (*fmt_ctx) {
        avformat_close_input(fmt_ctx);
        *fmt_ctx = NULL;
    }
    if (*dec_ctx) {
        avcodec_free_context(dec_ctx);
        *dec_ctx = NULL;
    }
}

static int get_frame_from_fifo(IAudioDecoder_Opaque *decoder,
        short *buffer, const int buffer_size_in_short) {
    int ret = -1;
    if (NULL == decoder)
        return ret;

    int nb_samples = buffer_size_in_short / decoder->dst_nb_channels;
    if (nb_samples > decoder->max_nb_copy_samples) {
        decoder->max_nb_copy_samples = nb_samples;
        ret = AllocateSampleBuffer(&(decoder->copy_buffer), decoder->dst_nb_channels,
                                   decoder->max_nb_copy_samples, OUT_SAMPLE_FMT);
        if (ret < 0) goto end;
    }

    memset(buffer, 0, sizeof(short) * buffer_size_in_short);
    ret = AudioFifoGet(decoder->audio_fifo, nb_samples, (void**)decoder->copy_buffer);
    if (ret < 0) goto end;

    ret = ret * decoder->dst_nb_channels;
    memcpy(buffer, decoder->copy_buffer[0], sizeof(short) * ret);
    set_gain(buffer, ret, decoder->volume_fix);

end:
    return ret;
}

static int resample_audio(IAudioDecoder_Opaque *decoder) {
    int ret = -1;
    if (NULL == decoder)
        return ret;

    decoder->dst_nb_samples = swr_get_out_samples(decoder->swr_ctx, decoder->audio_frame->nb_samples);
    if (decoder->dst_nb_samples > decoder->max_dst_nb_samples) {
        decoder->max_dst_nb_samples = decoder->dst_nb_samples;
        ret = AllocateSampleBuffer(&(decoder->dst_data), decoder->dst_nb_channels,
                               decoder->max_dst_nb_samples, OUT_SAMPLE_FMT);
        if (ret < 0) {
            LogError("%s av_samples_alloc error, error code = %d.\n", __func__, ret);
            goto end;
        }
    }
    // Convert to destination format
    ret = decoder->dst_nb_samples =
        swr_convert(decoder->swr_ctx, decoder->dst_data, decoder->dst_nb_samples,
                    (const uint8_t **)decoder->audio_frame->data,
                    decoder->audio_frame->nb_samples);
    if (ret < 0) {
        LogError("%s swr_convert error, error code = %d.\n", __func__, ret);
        goto end;
    }

end:
    return ret < 0 ? ret : 0;
}

static int seekTo_l(IAudioDecoder_Opaque *decoder) {
    int ret = 0;
    int64_t start_time = decoder->fmt_ctx->start_time;
    int64_t start_pos = (start_time != AV_NOPTS_VALUE ? start_time : 0);
    int64_t seek_pos = start_pos + milliseconds_to_fftime(
        decoder->seek_pos_ms + decoder->crop_start_time_in_ms);

    ret = avformat_seek_file(decoder->fmt_ctx, -1, INT64_MIN, seek_pos,
        INT64_MAX, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        LogError("%s: error while seeking.\n", __func__);
        goto end;
    } else {
        LogInfo("%s: avformat_seek_file success.\n", __func__);
        AudioFifoReset(decoder->audio_fifo);
    }

end:
    decoder->seek_pos_ms = 0;
    decoder->seek_req = false;
    return ret;
}

static int read_audio_packet(IAudioDecoder_Opaque *decoder,
        AVPacket *pkt) {
    int ret = -1;
    if (NULL == decoder)
        return ret;

    InitPacket(pkt);

    if (decoder->seek_req) seekTo_l(decoder);
    while (1) {
        ret = av_read_frame(decoder->fmt_ctx, pkt);
        if (ret < 0) {
            if (AVERROR_EOF != ret) {
                LogError("%s av_read_frame error, error code = %d.\n", __func__, ret);
            }
            break;
        }
        if (decoder->audio_stream_index == pkt->stream_index) break;
        av_packet_unref(pkt);
    }

    return ret;
}

static bool detect_EOF(IAudioDecoder_Opaque *decoder, AVPacket *pkt) {
    bool ret = false;
    if (NULL == decoder)
        return true;

    AVStream *audio_stream = decoder->fmt_ctx->streams[decoder->audio_stream_index];
    int cur_pos_in_ms = pkt->dts * 1000 * av_q2d(audio_stream->time_base);
    if(cur_pos_in_ms >= decoder->crop_start_time_in_ms + decoder->duration_ms) {
        ret = true;
    } else {
        ret = false;
    }

    return ret;
}

static int decode_audio_frame(IAudioDecoder_Opaque *decoder) {
    int ret = -1;
    if (NULL == decoder)
        return ret;

    AVPacket input_pkt;

    if (!decoder->dec_ctx) return kNullPointError;

    ret = read_audio_packet(decoder, &input_pkt);
    if (ret < 0) goto end;

    if (detect_EOF(decoder, &input_pkt)) {
        ret = AVERROR_EOF;
        goto end;
    }

    // If the packet is audio packet, decode and convert it to frame, then put
    // the frame into audio_fifo.
    if (decoder->audio_stream_index == input_pkt.stream_index) {
        ret = avcodec_send_packet(decoder->dec_ctx, &input_pkt);
        if (ret < 0) {
            LogError("%s Error submitting the packet to the decoder, error code = %d.\n", __func__, ret);
            goto end;
        }

        while (ret >= 0) {
            ret = avcodec_receive_frame(decoder->dec_ctx, decoder->audio_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                ret = 0;
                goto end;
            } else if (ret < 0) {
                LogError("%s Error during avcodec_receive_frame, error code = %d.\n", __func__, ret);
                goto end;
            }

            if (decoder->swr_ctx) {
                ret = resample_audio(decoder);
                if (ret < 0) goto end;
                ret = AudioFifoPut(decoder->audio_fifo, decoder->dst_nb_samples,
                                   (void **)decoder->dst_data);
                if (ret < 0) goto end;
            } else {
                ret =
                    AudioFifoPut(decoder->audio_fifo, decoder->audio_frame->nb_samples,
                                 (void **)decoder->audio_frame->data);
                if (ret < 0) goto end;
            }
        }
    }

end:
    av_packet_unref(&input_pkt);
    return ret < 0 ? ret : 0;
}

static int open_audio_file(IAudioDecoder_Opaque *decoder,
        const char *file_addr) {
    LogInfo("%s\n", __func__);
    int ret = -1;
    AVCodecContext *avctx = NULL;
    if (NULL == decoder)
        return ret;

    ret = CopyString(file_addr, &(decoder->file_addr));
    if (ret < 0) goto end;

    ret = OpenInputMediaFile(&(decoder->fmt_ctx), decoder->file_addr);
    if (ret < 0) goto end;

    ret = decoder->audio_stream_index = FindBestStream(decoder->fmt_ctx, AVMEDIA_TYPE_AUDIO);
    if (ret < 0) goto end;

    ret = FindAndOpenDecoder(decoder->fmt_ctx, &(decoder->dec_ctx), decoder->audio_stream_index);
    if (ret < 0) goto end;

    ret = InitResampler(decoder->dec_ctx->channels, decoder->dst_nb_channels,
                        decoder->dec_ctx->sample_rate, decoder->dst_sample_rate_in_Hz,
                        decoder->dec_ctx->sample_fmt, OUT_SAMPLE_FMT,
                        &(decoder->swr_ctx));
    if (ret < 0) goto end;

    return 0;

end:
    free_input_media_context(&(decoder->fmt_ctx), &avctx);
    return ret;
}

static void decoder_flush(IAudioDecoder_Opaque *decoder)
{
    LogInfo("%s.\n", __func__);
    if (NULL == decoder)
        return;

    decoder->flush = true;
    int ret = 0;
    ret = avcodec_send_packet(decoder->dec_ctx, NULL);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        LogInfo("%s the decoder has been flushed, exit.\n", __func__);
        return;
    }

    while (1) {
        ret = avcodec_receive_frame(decoder->dec_ctx, decoder->audio_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            LogError("%s Error during avcodec_receive_frame, error code = %d.\n", __func__, ret);
            break;
        }

        if (decoder->swr_ctx) {
            ret = resample_audio(decoder);
            if (ret < 0) break;
            ret = AudioFifoPut(decoder->audio_fifo, decoder->dst_nb_samples,
                        (void **)decoder->dst_data);
            if (ret < 0) break;
        } else {
            ret = AudioFifoPut(decoder->audio_fifo, decoder->audio_frame->nb_samples,
                        (void **)decoder->audio_frame->data);
            if (ret < 0) break;
        }
    }
}

static int get_duration_l(IAudioDecoder_Opaque *decoder) {
    int duration_ms;
    AVStream *audio_stream = decoder->fmt_ctx->streams[decoder->audio_stream_index];
    if (audio_stream->duration != AV_NOPTS_VALUE) {
        duration_ms = av_rescale_q(audio_stream->duration,
            audio_stream->time_base, AV_TIME_BASE_Q) / 1000;
    } else {
        duration_ms = fftime_to_milliseconds(decoder->fmt_ctx->duration);
    }
    return duration_ms;
}

static void init_decoder_params(IAudioDecoder_Opaque *decoder,
    int sample_rate, int channels, float volume_flp) {
    if (NULL == decoder)
        return;

    FFmpegDecoder_free(decoder);
    decoder->crop_start_time_in_ms = 0;
    decoder->crop_end_time_in_ms = 0;
    decoder->dst_sample_rate_in_Hz = sample_rate;
    decoder->dst_nb_channels = channels;
    decoder->dst_bits_per_sample = BITS_PER_SAMPLE_16;
    decoder->max_nb_copy_samples = MAX_NB_SAMPLES;
    decoder->max_dst_nb_samples = MAX_NB_SAMPLES;
    decoder->dst_nb_samples = MAX_NB_SAMPLES;
    decoder->audio_stream_index = -1;
    decoder->seek_pos_ms = 0;
    decoder->seek_req = false;
    decoder->flush = false;
    decoder->duration_ms = 0;
    decoder->volume_flp = volume_flp;
    decoder->volume_fix = (short)(32767 * volume_flp);
    decoder->decode_completed = false;
}

static void init_timings_params(IAudioDecoder_Opaque *decoder,
    int crop_start_time_ms, int crop_end_time_ms) {
    if (!decoder) return;

    decoder->duration_ms = get_duration_l(decoder);
    decoder->crop_start_time_in_ms = crop_start_time_ms < 0 ? 0 :
        (crop_start_time_ms > decoder->duration_ms ? decoder->duration_ms : crop_start_time_ms);
    decoder->crop_end_time_in_ms = crop_end_time_ms < 0 ? 0 :
        (crop_end_time_ms > decoder->duration_ms ? decoder->duration_ms : crop_end_time_ms);

    decoder->duration_ms = decoder->crop_end_time_in_ms - decoder->crop_start_time_in_ms;
    LogInfo("%s crop_start_time %d crop_end_time %d duration %d.\n", __func__, decoder->crop_start_time_in_ms,
        decoder->crop_end_time_in_ms, decoder->duration_ms);
}

static int init_decoder(IAudioDecoder_Opaque *decoder,
        const char *file_addr, int sample_rate, int channels, float volume_flp) {
    LogInfo("%s\n", __func__);
    int ret = -1;
    char *tmp_file_addr = NULL;
    if (NULL == decoder || NULL == file_addr)
        return ret;

    if ((ret = CopyString(file_addr, &tmp_file_addr)) < 0) {
        LogError("%s CopyString failed\n", __func__);
        goto end;
    }

    ret = CheckSampleRateAndChannels(sample_rate, channels);
    if (ret < 0) goto end;

    init_decoder_params(decoder, sample_rate, channels, volume_flp);

    // Allocate sample buffer for resampler
    ret = AllocateSampleBuffer(&(decoder->dst_data), channels,
                               decoder->max_dst_nb_samples, OUT_SAMPLE_FMT);
    if (ret < 0) goto end;

    // Allocate sample buffer for copy_buffer
    ret = AllocateSampleBuffer(&(decoder->copy_buffer), channels,
                               decoder->max_nb_copy_samples, OUT_SAMPLE_FMT);
    if (ret < 0) goto end;

    // Allocate buffer for audio frame
    decoder->audio_frame = av_frame_alloc();
    if (NULL == decoder->audio_frame) {
        LogError("%s Could not allocate input audio frame\n", __func__);
        ret = AVERROR(ENOMEM);
        goto end;
    }

    // Allocate buffer for audio fifo
    decoder->audio_fifo = av_audio_fifo_alloc(OUT_SAMPLE_FMT, channels, 1);
    if (NULL == decoder->audio_fifo) {
        LogError("%s Could not allocate audio FIFO\n", __func__);
        ret = AVERROR(ENOMEM);
        goto end;
    }

    if ((ret = open_audio_file(decoder, tmp_file_addr)) < 0) {
        LogError("%s open_audio_file failed\n", __func__);
        goto end;
    }
    decoder->duration_ms = get_duration_l(decoder);

    if (tmp_file_addr) {
        free(tmp_file_addr);
    }
    return 0;
end:
    if (tmp_file_addr) {
        free(tmp_file_addr);
    }
    FFmpegDecoder_free(decoder);
    return ret;
}

static void FFmpegDecoder_free(IAudioDecoder_Opaque *decoder) {
    LogInfo("%s\n", __func__);
    if (NULL == decoder)
        return;

    if (decoder->audio_fifo) {
        av_audio_fifo_free(decoder->audio_fifo);
        decoder->audio_fifo = NULL;
    }
    if (decoder->copy_buffer) {
        av_freep(&(decoder->copy_buffer[0]));
        av_freep(&(decoder->copy_buffer));
    }
    free_input_media_context(&(decoder->fmt_ctx), &(decoder->dec_ctx));
    if (decoder->audio_frame) {
        av_frame_free(&(decoder->audio_frame));
        decoder->audio_frame = NULL;
    }
    if (decoder->swr_ctx) {
        swr_free(&(decoder->swr_ctx));
        decoder->swr_ctx = NULL;
    }
    if (decoder->dst_data) {
        av_freep(&(decoder->dst_data[0]));
        av_freep(&(decoder->dst_data));
    }
    if (decoder->file_addr) {
        av_freep(&(decoder->file_addr));
        decoder->file_addr = NULL;
    }
}

static int FFmpegDecoder_get_pcm_frame(
        IAudioDecoder_Opaque *decoder, short *buffer,
        const int buffer_size_in_short, bool loop) {
    int ret = -1;
    if (!decoder || !buffer || buffer_size_in_short < 0)
        return ret;
    if (decoder->decode_completed) return PCM_FILE_EOF;

    while (av_audio_fifo_size(decoder->audio_fifo) * decoder->dst_nb_channels <
           buffer_size_in_short) {
        ret = decode_audio_frame(decoder);
        if (ret < 0) {
            if (ret == AVERROR_EOF && !decoder->flush) {
                decoder_flush(decoder);
            }
            if (ret == AVERROR_EOF && loop) {
                int crop_start_time_in_ms = decoder->crop_start_time_in_ms;
                int crop_end_time_in_ms = decoder->crop_end_time_in_ms;
                if ((ret = init_decoder(decoder, decoder->file_addr,
                        decoder->dst_sample_rate_in_Hz, decoder->dst_nb_channels,
                        decoder->volume_flp)) < 0) {
                    LogError("%s init_decoder failed\n", __func__);
                    goto end;
                }
                if (FFmpegDecoder_set_crop_pos(decoder,
                    crop_start_time_in_ms, crop_end_time_in_ms) < 0) {
                    LogError("%s FFmpegDecoder_set_crop_pos failed.\n", __func__);
                    goto end;
                }
            } else if (ret == AVERROR_EOF && 0 != av_audio_fifo_size(decoder->audio_fifo)) {
                break;
            } else {
                decoder->decode_completed = true;
                goto end;
            }
        }
    }
    return get_frame_from_fifo(decoder, buffer, buffer_size_in_short);

end:
    if (ret == AVERROR_EOF) ret = PCM_FILE_EOF;
    return ret;
}

static int FFmpegDecoder_seekTo(IAudioDecoder_Opaque *decoder,
        int seek_pos_ms) {
    LogInfo("%s seek_pos_ms %d\n", __func__, seek_pos_ms);
    if (NULL == decoder)
        return -1;

    decoder->decode_completed = false;
    decoder->seek_req = true;
    decoder->seek_pos_ms = seek_pos_ms < 0 ? 0 : seek_pos_ms;
    int file_duration = decoder->duration_ms;
    if (file_duration > 0 && decoder->seek_pos_ms != file_duration) {
        decoder->seek_pos_ms = decoder->seek_pos_ms % file_duration;
    }
    LogInfo("%s decoder->seek_pos_ms %d, file_duration %d\n", __func__,
        decoder->seek_pos_ms, file_duration);

    AudioFifoReset(decoder->audio_fifo);
    decoder->flush = false;
    return 0;
}

static int FFmpegDecoder_set_crop_pos(
    IAudioDecoder_Opaque *decoder, int crop_start_time_ms,
    int crop_end_time_ms) {
    LogInfo("%s\n", __func__);
    int ret = -1;
    if (!decoder || !decoder->fmt_ctx)
        return ret;

    init_timings_params(decoder, crop_start_time_ms, crop_end_time_ms);

    int64_t start_time = decoder->fmt_ctx->start_time;
    start_time = (start_time != AV_NOPTS_VALUE ? start_time : 0);
    int64_t start_pos = start_time + milliseconds_to_fftime(decoder->crop_start_time_in_ms);
    ret = avformat_seek_file(decoder->fmt_ctx, -1, INT64_MIN, start_pos,
        INT64_MAX, AVSEEK_FLAG_BACKWARD);

    return ret < 0 ? ret : decoder->duration_ms;
}

IAudioDecoder *FFmpegDecoder_create(const char *file_addr,
    int dst_sample_rate, int dst_channels, float volume_flp) {
    LogInfo("%s.\n", __func__);
    int ret = -1;
    if (NULL == file_addr) {
        LogError("%s file_addr is NULL.\n", __func__);
        return NULL;
    }

    IAudioDecoder *decoder = IAudioDecoder_create(sizeof(IAudioDecoder_Opaque));
    if (NULL == decoder) {
        LogError("%s Could not allocate IAudioDecoder.\n", __func__);
        goto end;
    }

    decoder->func_set_crop_pos = FFmpegDecoder_set_crop_pos;
    decoder->func_seekTo = FFmpegDecoder_seekTo;
    decoder->func_get_pcm_frame = FFmpegDecoder_get_pcm_frame;
    decoder->func_free = FFmpegDecoder_free;

    IAudioDecoder_Opaque *opaque = decoder->opaque;
    if ((ret = init_decoder(opaque, file_addr, dst_sample_rate,
            dst_channels, volume_flp)) < 0) {
        LogError("%s init_decoder failed.\n", __func__);
        goto end;
    }
    decoder->out_sample_rate = opaque->dst_sample_rate_in_Hz;
    decoder->out_nb_channels = opaque->dst_nb_channels;
    decoder->out_bits_per_sample = opaque->dst_bits_per_sample;
    decoder->duration_ms = opaque->duration_ms;

    return decoder;
end:
    if (decoder) {
        IAudioDecoder_freep(&decoder);
    }
    return NULL;
}
#endif
