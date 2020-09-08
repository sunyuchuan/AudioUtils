#include "audio_decoder_factory.h"
#include "ffmpeg_decoder.h"
#include "pcm_decoder.h"

IAudioDecoder *audio_decoder_create(const char *file_addr,
    int src_sample_rate, int src_nb_channels, int dst_sample_rate,
    int dst_nb_channels, float volume_flp, enum DecoderType decoder_type)
{
    IAudioDecoder *decoder = NULL;
    switch(decoder_type) {
        case DECODER_FFMPEG:
            decoder = FFmpegDecoder_create(file_addr, dst_sample_rate,
                            dst_nb_channels, volume_flp);
        break;
        default:
            decoder = PcmDecoder_create(file_addr, src_sample_rate,
                src_nb_channels, src_sample_rate, dst_nb_channels, volume_flp);
        break;
    }

    return decoder;
}

