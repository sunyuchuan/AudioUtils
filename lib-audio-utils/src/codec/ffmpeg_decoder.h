#if defined(__ANDROID__) || defined (__linux__)
#ifndef FFMPEG_DECODER_H
#define FFMPEG_DECODER_H
#include "idecoder.h"

IAudioDecoder *FFmpegDecoder_create(const char *file_addr,
    int dst_sample_rate, int dst_channels, float volume_flp);

#endif // FFMPEG_DECODER_H
#endif // defined(__ANDROID__) || defined (__linux__)
