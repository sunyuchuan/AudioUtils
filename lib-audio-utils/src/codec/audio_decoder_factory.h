#ifndef AUDIO_DECODER_FACTORY_H
#define AUDIO_DECODER_FACTORY_H
#include "idecoder.h"

enum DecoderType {
    DECODER_NONE = -1,
    DECODER_FFMPEG,
    DECODER_PCM
};

IAudioDecoder *audio_decoder_create(const char *file_addr,
    int src_sample_rate, int src_nb_channels, int dst_sample_rate,
    int dst_nb_channels, float volume_flp, enum DecoderType decoder_type);
#endif //AUDIO_DECODER_FACTORY_H