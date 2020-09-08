#if defined(__ANDROID__) || defined (__linux__)

#ifndef _MUXER_CONFIG_H_
#define _MUXER_CONFIG_H_
#include "codec/ffmpeg_utils.h"

enum EncoderType {
    ENCODER_NONE = -1,
    ENCODER_FFMPEG,
    ENCODER_MEDIA_CODEC,
    ENCODER_IOS_HW,
};

typedef struct MuxerConfig {
    int src_sample_rate_in_Hz;
    int src_nb_channels;
    int dst_sample_rate_in_Hz;
    int dst_nb_channels;
    int dst_bit_rate;
    char *muxer_name;
    char *mime;
    char *output_filename;
    enum AVSampleFormat src_sample_fmt;
    enum AVCodecID codec_id;
    enum EncoderType encoder_type;
} MuxerConfig;

#endif
#endif
