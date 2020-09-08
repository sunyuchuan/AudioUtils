#ifndef PCM_DECODER_H
#define PCM_DECODER_H
#include "idecoder.h"

IAudioDecoder *PcmDecoder_create(const char *file_addr,
    int src_sample_rate, int src_nb_channels, int dst_sample_rate,
    int dst_nb_channels, float volume_flp);

#endif // PCM_DECODER_H