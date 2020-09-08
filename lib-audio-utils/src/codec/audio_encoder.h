#if defined(__ANDROID__) || defined (__linux__)

#ifndef __AUDIO_ENCODER_H__
#define __AUDIO_ENCODER_H__

#include "libavutil/frame.h"
#include "libavcodec/avcodec.h"

typedef struct Encoder_Opaque Encoder_Opaque;
typedef struct Encoder Encoder;
typedef enum FF_AVMediaType FF_AVMediaType;

//ugly ios
enum FF_AVMediaType {
    FF_AVMEDIA_TYPE_UNKNOWN = -1,  ///< Usually treated as AVMEDIA_TYPE_DATA
    FF_AVMEDIA_TYPE_VIDEO,
    FF_AVMEDIA_TYPE_AUDIO,
    FF_AVMEDIA_TYPE_DATA,          ///< Opaque data information usually continuous
    FF_AVMEDIA_TYPE_SUBTITLE,
    FF_AVMEDIA_TYPE_ATTACHMENT,    ///< Opaque data information usually sparse
    FF_AVMEDIA_TYPE_NB
};

#define MIME_VIDEO_AVC "video/avc"
#define MIME_AUDIO_AAC "audio/aac"
#define MIME_AUDIO_WAV "audio/wav"
#define MUXER_AUDIO_WAV "wav"
#define MUXER_AUDIO_MP4 "mp4"

struct Encoder {
    Encoder_Opaque *opaque;

    int (*func_config)(Encoder *ctx, AVDictionary *opt);
    int (*func_get_frame_size)(Encoder *ctx);
    int (*func_encode_frame)(Encoder *ctx, AVFrame *frame, AVPacket *pkt, int *got_packet_ptr);
    int (*func_destroy)(Encoder *ctx);
};

Encoder *ff_encoder_alloc(size_t opaque_size);
void ff_encoder_free(Encoder *encoder);
void ff_encoder_free_p(Encoder **encoder);

int ff_encoder_config(Encoder *encoder, AVDictionary *opt);
int ff_encoder_get_frame_size(Encoder *encoder);
int ff_encoder_encode_frame(Encoder *encoder, AVFrame *frame, AVPacket *pkt, int *got_packet_ptr);

#endif
#endif
