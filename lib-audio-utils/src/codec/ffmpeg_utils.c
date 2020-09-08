#if defined(__ANDROID__) || defined (__linux__)

#include "ffmpeg_utils.h"
#include "error_def.h"
#include "log.h"

#define XM_FFMPEG_LOG_TAG "FFMPEG"

inline static int LogLevelAvToXm(int av_level)
{
    int xm_level = LOG_LEVEL_TRACE;
    if      (av_level <= AV_LOG_PANIC)      xm_level = LOG_LEVEL_PANIC;
    else if (av_level <= AV_LOG_FATAL)      xm_level = LOG_LEVEL_FATAL;
    else if (av_level <= AV_LOG_ERROR)      xm_level = LOG_LEVEL_ERROR;
    else if (av_level <= AV_LOG_WARNING)    xm_level = LOG_LEVEL_WARNING;
    else if (av_level <= AV_LOG_INFO)       xm_level = LOG_LEVEL_INFO;
    else if (av_level <= AV_LOG_VERBOSE)    xm_level = LOG_LEVEL_VERBOSE;
    else if (av_level <= AV_LOG_DEBUG)      xm_level = LOG_LEVEL_DEBUG;
    else if (av_level <= AV_LOG_TRACE)      xm_level = LOG_LEVEL_TRACE;
    else                                    xm_level = LOG_LEVEL_TRACE;
    return xm_level;
}

inline static int LogLevelXmToAv(int xm_level)
{
    int av_level = LOG_LEVEL_TRACE;
    if      (xm_level >= LOG_LEVEL_QUIET)   av_level = AV_LOG_QUIET;
    else if (xm_level >= LOG_LEVEL_FATAL)    av_level = AV_LOG_FATAL;
    else if (xm_level >= LOG_LEVEL_ERROR)    av_level = AV_LOG_ERROR;
    else if (xm_level >= LOG_LEVEL_WARNING)	   av_level = AV_LOG_WARNING;
    else if (xm_level >= LOG_LEVEL_INFO)	   av_level = AV_LOG_INFO;
    else if (xm_level >= LOG_LEVEL_DEBUG)    av_level = AV_LOG_DEBUG;
    else if (xm_level >= LOG_LEVEL_VERBOSE)  av_level = AV_LOG_VERBOSE;
    else if (xm_level >= LOG_LEVEL_TRACE)  av_level = AV_LOG_TRACE;
    else                                    av_level = AV_LOG_TRACE;
    return av_level;
}

static void LogCallbackBrief(__attribute__((unused)) void *ptr, int level, const char *fmt, va_list vl)
{
    if (level > av_log_get_level())
        return;

    int xmlv __attribute__((unused)) = LogLevelAvToXm(level);
    FFMPEGLOG(xmlv, XM_FFMPEG_LOG_TAG, fmt, vl);
}

void SetFFmpegLogLevel(int log_level)
{
    int av_level = LogLevelXmToAv(log_level);
    av_log_set_level(av_level);
    av_log_set_callback(LogCallbackBrief);
}

void RegisterFFmpeg() {
    static bool init = false;
    if (!init) {
        init = true;
        av_register_all();
        avformat_network_init();
    }
}

int CopyString(const char* src, char** dst) {
    if (NULL != *dst) {
        if (strcmp(src, *dst) == 0) return 0;
        av_freep(dst);
        *dst = NULL;
    }
    *dst = av_strdup(src);
    if (NULL == *dst) {
        LogError("CopyString Error, av_strdup error!\n");
        return AVERROR(ENOMEM);
    }
    return 0;
}

void InitPacket(AVPacket* packet) {
    av_init_packet(packet);
    packet->data = NULL;
    packet->size = 0;
}

int CheckSampleRateAndChannels(const int sample_rate_in_Hz,
                               const int nb_channels) {
    if (sample_rate_in_Hz != 8000 && sample_rate_in_Hz != 16000 &&
        sample_rate_in_Hz != 11025 && sample_rate_in_Hz != 22050 &&
        sample_rate_in_Hz != 32000 && sample_rate_in_Hz != 44100 &&
        sample_rate_in_Hz != 48000 && sample_rate_in_Hz != 50000 &&
        sample_rate_in_Hz != 50400 && sample_rate_in_Hz != 64000 &&
        sample_rate_in_Hz != 88200 && sample_rate_in_Hz != 96000) {
        LogError("Not support sample_rate %d Hz!\n", sample_rate_in_Hz);
        return kSampleRateNotSupport;
    }
    if (1 != nb_channels && 2 != nb_channels) {
        LogError("Number of channels(%d) Error, only support Mono and Stereo!\n",
                nb_channels);
        return kChannelsNotSupport;
    }
    return 0;
}

int OpenInputMediaFile(AVFormatContext** fmt_ctx, const char* filename) {
    int ret = 0;

    if (*fmt_ctx) avformat_close_input(fmt_ctx);

    // Open the input file to read from it.
    ret = avformat_open_input(fmt_ctx, filename, NULL, NULL);
    if (ret < 0) {
        LogError("Could not open input file '%s', error(%s), error code(%d)\n",
                filename, av_err2str(ret), ret);
        goto end;
    }

    // Get information on the input file (number of streams etc.).
    ret = avformat_find_stream_info(*fmt_ctx, NULL);
    if (ret < 0) {
        LogError("Could not open find stream info, error(%s), error code(%d)\n",
                av_err2str(ret), ret);
        goto end;
    }

    av_dump_format(*fmt_ctx, 0, filename, 0);

end:
    return ret;
}

#if defined(__ANDROID__) || defined (__linux__)
int FindFirstStream(AVFormatContext* fmt_ctx, enum AVMediaType type) {
#else
int FindFirstStream(AVFormatContext* fmt_ctx, int type) {
#endif
    int stream_index = -1;

    for (unsigned int i = 0; i < fmt_ctx->nb_streams; ++i) {
        if (type == fmt_ctx->streams[i]->codecpar->codec_type) {
            stream_index = i;
            break;
        }
    }

    if (stream_index == -1) {
        LogError("Stream not found!\n");
        stream_index = AVERROR_STREAM_NOT_FOUND;
    }

    return stream_index;
}

#if defined(__ANDROID__) || defined (__linux__)
int FindBestStream(AVFormatContext* fmt_ctx, enum AVMediaType type) {
#else
int FindBestStream(AVFormatContext* fmt_ctx, int type) {
#endif
    int stream_index = -1;

    stream_index = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (stream_index < 0) {
        LogError("Could not find %s stream in input file\n",
                av_get_media_type_string(type));
    }

    return stream_index;
}

int AllocDecodeFrame(AVFrame** decode_frame) {
    if (*decode_frame) av_frame_free(decode_frame);

    *decode_frame = av_frame_alloc();
    if (NULL == *decode_frame) {
        LogError("Could not allocate input audio frame\n");
        return AVERROR(ENOMEM);
    }
    return 0;
}

double GetStreamStartTime(AVFormatContext* fmt_ctx, const unsigned int stream_index) {
    double result = 0.0;
    if (stream_index > fmt_ctx->nb_streams) return result;
    if (fmt_ctx->streams[stream_index]->start_time != AV_NOPTS_VALUE)
        result = (double)(fmt_ctx->streams[stream_index]->start_time) *
                 av_q2d(fmt_ctx->streams[stream_index]->time_base);
    LogInfo("GetStreamStartTime result = %lf.\n", result);
    return result;
}

int FindAndOpenDecoder(const AVFormatContext* fmt_ctx, AVCodecContext** dec_ctx,
                       const int stream_index) {
    int ret = 0;
    AVCodecContext* avctx = NULL;
    AVCodec* input_codec = NULL;

    if (*dec_ctx) avcodec_free_context(dec_ctx);
    // Find a decoder for the stream.
    input_codec = avcodec_find_decoder(
        fmt_ctx->streams[stream_index]->codecpar->codec_id);
    if (NULL == input_codec) {
        LogError("Could not find input codec\n");
        ret = AVERROR_DECODER_NOT_FOUND;
        goto end;
    }

    // Allocate a new decoding context
    avctx = avcodec_alloc_context3(input_codec);
    if (NULL == avctx) {
        LogError("Could not allocate a decoding context\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }
    if (input_codec->capabilities & AV_CODEC_CAP_TRUNCATED)
        avctx->flags |= AV_CODEC_FLAG_TRUNCATED;

    // Initialize the stream parameters with demuxer information
    av_codec_set_pkt_timebase(avctx, fmt_ctx->streams[stream_index]->time_base);
    ret = avcodec_parameters_to_context(
        avctx, fmt_ctx->streams[stream_index]->codecpar);
    if (ret < 0) goto end;

    // Open the decoder for the stream to use it later.
    ret = avcodec_open2(avctx, input_codec, NULL);
    if (ret < 0) {
        LogError("Could not open input codec(%s), error code(%d)\n",
                av_err2str(ret), ret);
        goto end;
    }

    // Save the decoder context for easier access later.
    *dec_ctx = avctx;

end:
    if (ret < 0 && avctx) avcodec_free_context(&avctx);
    return ret;
}

void InitAdtsHeader(uint8_t* adts_header, const int sample_rate_in_Hz,
                    const int nb_channels) {
    int profile = 2;    // AAC LC
    int freqIdx = 0x4;  // 44.1KHz
    switch (sample_rate_in_Hz) {
        case 96000:
            freqIdx = 0x0;
            break;
        case 88200:
            freqIdx = 0x1;
            break;
        case 64000:
            freqIdx = 0x2;
            break;
        case 48000:
            freqIdx = 0x3;
            break;
        case 44100:
            freqIdx = 0x4;
            break;
        case 32000:
            freqIdx = 0x5;
            break;
        default:
            freqIdx = 0x4;
            break;
    }
    adts_header[0] = 0xFF;
    adts_header[1] = 0xF9;
    adts_header[2] = ((profile - 1) << 6) + (freqIdx << 2) + (nb_channels >> 2);
    adts_header[6] = 0xFC;
}

void ResetAdtsHeader(uint8_t* adts_header, const int nb_channels,
                     const int packet_size) {
    // adts_header[0] = 0xFF;
    // adts_header[1] = 0xF9;
    // adts_header[2] = ((profile - 1) << 6) + (freqIdx << 2) +
    // (nb_channels >> 2);
    adts_header[3] = ((nb_channels & 3) << 6) + ((packet_size + 7) >> 11);
    adts_header[4] = ((packet_size + 7) & 0x7FF) >> 3;
    adts_header[5] = (((packet_size + 7) & 7) << 5) + 0x1F;
    // adts_header[6] = 0xFC;
}

int AddAudioStream(AVFormatContext *ofmt_ctx, AVCodecContext *enc_ctx) {
    int ret = -1;
    AVStream *out_stream = NULL;
    if(!(out_stream = avformat_new_stream(ofmt_ctx, enc_ctx->codec)))
    {
        LogError("avformat_new_stream failed.\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }
    out_stream->id = ofmt_ctx->nb_streams - 1;
    out_stream->time_base = enc_ctx->time_base;

    if((ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx)) < 0)
    {
        LogError("Could not initialize out_stream parameters.\n");
        goto end;
    }

    enc_ctx->codec_tag = 0;
    if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    ret = out_stream->id;
end:
    return ret;
}

int FindAndOpenAudioEncoder(AVCodecContext** enc_ctx,
                            const enum AVCodecID codec_id, const int bit_rate,
                            const int nb_channels,
                            const int sample_rate_in_Hz) {
    int ret = 0;
    AVCodec* codec = NULL;

    if (*enc_ctx) avcodec_free_context(enc_ctx);

    // Find the encoder.
    codec = avcodec_find_encoder(codec_id);
    if (NULL == codec) {
        LogError("Could not find encoder for '%s', error(%s) error code = %d\n",
                avcodec_get_name(codec_id), av_err2str(ret), ret);
        goto end;
    }

    *enc_ctx = avcodec_alloc_context3(codec);
    if (NULL == *enc_ctx) {
        LogError("Could not allocate an encoding context\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }

    // Select other audio parameters supported by the encoder
    (*enc_ctx)->cutoff = 18000;
    (*enc_ctx)->bit_rate = bit_rate;
    (*enc_ctx)->channels = nb_channels;
    (*enc_ctx)->channel_layout = av_get_default_channel_layout(nb_channels);
    (*enc_ctx)->sample_fmt =
        codec->sample_fmts ? codec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
    if (codec->supported_samplerates) {
        (*enc_ctx)->sample_rate = 0;
        for (int i = 0; codec->supported_samplerates[i]; i++) {
            if (codec->supported_samplerates[i] == sample_rate_in_Hz)
                (*enc_ctx)->sample_rate = sample_rate_in_Hz;
        }
        if (0 == (*enc_ctx)->sample_rate) {
            LogError("The codec doesn't support the sample rate.\n");
            ret = kSampleRateNotSupport;
            goto end;
        }
    } else {
        (*enc_ctx)->sample_rate = sample_rate_in_Hz;
    }
    (*enc_ctx)->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    (*enc_ctx)->time_base = (AVRational){1, (*enc_ctx)->sample_rate};

    ret = avcodec_open2(*enc_ctx, codec, NULL);
    if (ret < 0) {
        LogError("Could not open output codec(%s), error code(%d)\n",
                av_err2str(ret), ret);
        goto end;
    }
    if (codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE) {
        (*enc_ctx)->frame_size = FRAME_SIZE;
    }

end:
    if (ret < 0 && *enc_ctx) avcodec_free_context(enc_ctx);
    return ret;
}

int FindAndOpenH264Encoder(AVCodecContext** enc_ctx, const int bit_rate,
                           const int frame_rate, const int gop_size,
                           const int width, const int height,
                           const enum AVPixelFormat pix_fmt) {
    int ret = 0;
    AVCodec* codec = NULL;

    if (*enc_ctx) avcodec_free_context(enc_ctx);

    // Find the encoder.
    codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (NULL == codec) {
        LogError("Could not find encoder for '%s', error(%s) error code = %d\n",
                avcodec_get_name(AV_CODEC_ID_H264), av_err2str(ret), ret);
        goto end;
    }

    *enc_ctx = avcodec_alloc_context3(codec);
    if (NULL == *enc_ctx) {
        LogError("Could not allocate an encoding context\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }

    // Select other audio parameters supported by the encoder
    (*enc_ctx)->codec_id = AV_CODEC_ID_H264;
    (*enc_ctx)->bit_rate = bit_rate;
    (*enc_ctx)->width = width;
    (*enc_ctx)->height = height;
    /* Be careful, time_base of stream will auto change */
    (*enc_ctx)->time_base = (AVRational){1, frame_rate};
    (*enc_ctx)->gop_size = gop_size;
    (*enc_ctx)->pix_fmt = pix_fmt;  // AV_PIX_FMT_YUV420P;  AV_PIX_FMT_NV12;
    av_opt_set((*enc_ctx)->priv_data, "preset", "slow", 0);

    ret = avcodec_open2(*enc_ctx, codec, NULL);
    if (ret < 0) {
        LogError("Could not open output codec(%s), error code(%d)\n",
                av_err2str(ret), ret);
        goto end;
    }

end:
    if (ret < 0 && *enc_ctx) avcodec_free_context(enc_ctx);
    return ret;
}

int AddStreamToFormat(AVFormatContext* fmt_ctx, AVCodecContext* enc_ctx) {
    int ret = 0;
    AVStream* stream = NULL;

    /** Create a new audio stream in the output file container. */
    stream = avformat_new_stream(fmt_ctx, NULL);
    if (NULL == stream) {
        LogError("Could not create new stream\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }
    stream->id = fmt_ctx->nb_streams - 1;
    stream->time_base = enc_ctx->time_base;

    /* Some formats want stream headers to be separate. */
    if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    ret = avcodec_parameters_from_context(stream->codecpar, enc_ctx);
    if (ret < 0) {
        LogError("Could not initialize stream parameters\n");
        goto end;
    }

end:
    return ret < 0 ? ret : stream->id;
}

int WriteFileHeader(AVFormatContext* ofmt_ctx, const char* filename) {
    int ret = 0;
    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            LogError("Could not open output file '%s'", filename);
            goto end;
        }
    }

    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0)
        LogError("Could not write output file header (error = %d)\n", ret);

end:
    return ret;
}

void WriteFileTrailer(AVFormatContext* ofmt_ctx) {
    if (ofmt_ctx) {
        int ret = av_write_trailer(ofmt_ctx);
        if (ret < 0)
            LogError("Could not write output file trailer (error = %d)\n", ret);
    }
}

int InitResampler(const int src_channels, const int dst_channels,
                  const int src_sample_rate, const int dst_sample_rate,
                  const enum AVSampleFormat src_sample_fmt,
                  const enum AVSampleFormat dst_sample_fmt,
                  SwrContext** swr_ctx) {
    int ret = 0;

    if (*swr_ctx) swr_free(swr_ctx);

    if (src_channels != dst_channels || src_sample_rate != dst_sample_rate ||
        src_sample_fmt != dst_sample_fmt) {
        *swr_ctx = swr_alloc_set_opts(
            NULL, av_get_default_channel_layout(dst_channels), dst_sample_fmt,
            dst_sample_rate, av_get_default_channel_layout(src_channels),
            src_sample_fmt, src_sample_rate, 0, NULL);
        if (NULL == *swr_ctx) {
            LogError("Could not allocate resample context\n");
            ret = AVERROR(ENOMEM);
        }
        // Open the resampler with the specified parameters.
        ret = swr_init(*swr_ctx);
        if (ret < 0) {
            LogError("swr_init error(%s) error code = %d\n", av_err2str(ret),
                    ret);
            goto end;
        }
    }

end:
    if (ret < 0 && *swr_ctx) swr_free(swr_ctx);
    return ret;
}

int AllocEncodeAudioFrame(AVFrame** audio_frame, const int nb_channels,
                          const int sample_rate_in_Hz, const int nb_samples,
                          const enum AVSampleFormat sample_fmt) {
    int ret = 0;

    if (*audio_frame) av_frame_free(audio_frame);

    *audio_frame = av_frame_alloc();
    if (NULL == *audio_frame) {
        LogError("Could not allocate output video frame\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }
    (*audio_frame)->nb_samples = nb_samples;
    (*audio_frame)->channel_layout = av_get_default_channel_layout(nb_channels);
    (*audio_frame)->format = sample_fmt;
    (*audio_frame)->sample_rate = sample_rate_in_Hz;

    ret = av_frame_get_buffer(*audio_frame, 0);
    if (ret < 0) {
        LogError("Could not allocate output frame samples(%s) error code = %d\n",
                av_err2str(ret), ret);
        goto end;
    }

end:
    if (ret < 0 && *audio_frame) av_frame_free(audio_frame);
    return ret;
}

int AllocEncodeVideoFrame(AVFrame** video_frame, const int width,
                          const int height, const enum AVPixelFormat pix_fmt) {
    int ret = 0;

    if (*video_frame) av_frame_free(video_frame);

    *video_frame = av_frame_alloc();
    if (NULL == *video_frame) {
        LogError("Could not allocate output video frame\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }
    (*video_frame)->width = width;
    (*video_frame)->height = height;
    (*video_frame)->format = pix_fmt;

    ret = av_frame_get_buffer(*video_frame, 1);
    if (ret < 0) {
        LogError("Could not allocate output video frame(%s) error code = %d\n",
                av_err2str(ret), ret);
        goto end;
    }
    /* make sure the frame data is writable */
    ret = av_frame_make_writable(*video_frame);
    if (ret < 0) {
        LogError("av_frame_make_writable error (%s) error code = %d\n",
                av_err2str(ret), ret);
        goto end;
    }

end:
    if (ret < 0 && *video_frame) av_frame_free(video_frame);
    return ret;
}

int AllocateSampleBuffer(uint8_t*** buffer, const int nb_channels,
                         const int nb_samples,
                         const enum AVSampleFormat sample_fmt) {
    if (*buffer) {
        av_freep(&((*buffer)[0]));
        av_freep(buffer);
    }
    int ret = av_samples_alloc_array_and_samples(buffer, NULL, nb_channels,
                                                 nb_samples, sample_fmt, 0);
    if (ret < 0) {
        LogError("Could not allocate source samples(%s) error code = %d\n",
                av_err2str(ret), ret);
        goto end;
    }
end:
    return ret;
}

int AllocAudioFifo(const enum AVSampleFormat sample_fmt, const int nb_channels,
                   AVAudioFifo** encode_fifo) {
    if (*encode_fifo) {
        av_audio_fifo_free(*encode_fifo);
        *encode_fifo = NULL;
    }

    *encode_fifo = av_audio_fifo_alloc(sample_fmt, nb_channels, 1);
    if (NULL == *encode_fifo) {
        LogError("Could not allocate FIFO\n");
        return AVERROR(ENOMEM);
    }
    return 0;
}

int AudioFifoPut(AVAudioFifo* fifo, const int nb_samples, void** buffer) {
    int ret = av_audio_fifo_write(fifo, buffer, nb_samples);
    if (ret < nb_samples)
        LogError("%s:%d Could not write data to FIFO(%s) error code = %d.\n",
                __FILE__, __LINE__, av_err2str(ret), ret);
    return ret < 0 ? ret : 0;
}

int AudioFifoGet(AVAudioFifo* fifo, const int nb_samples, void** buffer) {
    int ret = av_audio_fifo_read(fifo, buffer, nb_samples);
    if (ret < 0)
        LogError("%s:%d Could not get data from FIFO(%s) error code = %d\n",
                __FILE__, __LINE__, av_err2str(ret), ret);
    return ret;
}

void AudioFifoReset(AVAudioFifo* fifo) {
    if (fifo != NULL) av_audio_fifo_reset(fifo);
}

// void LogPacket(const AVFormatContext* fmt_ctx, const AVPacket* pkt) {
//     AVRational* time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

//     av_log(NULL, AV_LOG_INFO,
//            "stream_dts:%s pts:%s pts_time:%s dts:%s dts_time:%s duration:%s "
//            "duration_time:%s stream_index:%d\n",
//            av_ts2str(fmt_ctx->streams[pkt->stream_index]->cur_dts),
//            av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
//            av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
//            av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
//            pkt->stream_index);
// }

float UpdateFactorS16(const float factor, const int sum) {
    float result = factor;
    if (sum > 32767) {
        result = 32767.0f / (float)(sum);
    } else if (sum < -32768) {
        result = -32768.0f / (float)(sum);
    }
    if (factor < 1.0f) {
        result += (1.0f - factor) / 32.0f;
    }
    return result;
}

short GetSumS16(const int sum) {
    return sum < 0 ? (-32768 < sum ? sum : -32768)
                   : (sum < 32767 ? sum : 32767);
}

void MixBufferS16(const short* src_buffer1, const short* src_buffer2,
                  const int nb_mix_samples, const int nb_channels,
                  short* dst_buffer, float* left_factor, float* right_factor) {
    int sum = 0;
    for (int i = 0; i < nb_mix_samples; ++i) {
        if (1 == nb_channels) {
            sum = (src_buffer1[i] + src_buffer2[i]) * (*left_factor);
            *left_factor = UpdateFactorS16(*left_factor, sum);
            dst_buffer[i] = GetSumS16(sum);
        } else if (2 == nb_channels) {
            sum = (src_buffer1[i << 1] + src_buffer2[i << 1]) * (*left_factor);
            *left_factor = UpdateFactorS16(*left_factor, sum);
            dst_buffer[i << 1] = GetSumS16(sum);
            sum = (src_buffer1[(i << 1) + 1] + src_buffer2[(i << 1) + 1]) *
                  (*right_factor);
            *right_factor = UpdateFactorS16(*right_factor, sum);
            dst_buffer[(i << 1) + 1] = GetSumS16(sum);
        }
    }
}

void StereoToMonoS16(short* dst, short* src, const int nb_samples) {
    short* p = src;
    short* q = dst;
    int n = nb_samples;

    while (n >= 4) {
        q[0] = (p[0] + p[1]) >> 1;
        q[1] = (p[2] + p[3]) >> 1;
        q[2] = (p[4] + p[5]) >> 1;
        q[3] = (p[6] + p[7]) >> 1;
        q += 4;
        p += 8;
        n -= 4;
    }
    while (n > 0) {
        q[0] = (p[0] + p[1]) >> 1;
        q++;
        p += 2;
        n--;
    }
}

void MonoToStereoS16(short* dst, short* src, const int nb_samples) {
    short* p = src;
    short* q = dst;
    short v = 0;
    int n = nb_samples;
    //double sqrt2_div2 = 0.70710678118;

    while (n >= 4) {
        v = p[0];
        q[0] = v;
        q[1] = v;
        v = p[1];
        q[2] = v;
        q[3] = v;
        v = p[2];
        q[4] = v;
        q[5] = v;
        v = p[3];
        q[6] = v;
        q[7] = v;
        q += 8;
        p += 4;
        n -= 4;
    }
    while (n > 0) {
        v = p[0];
        q[0] = v;
        q[1] = v;
        q += 2;
        p += 1;
        n--;
    }
}
#endif
