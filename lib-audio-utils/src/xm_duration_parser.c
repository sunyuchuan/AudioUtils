#include "xm_duration_parser.h"
#include "codec/duration_parser.h"

int get_file_duration_ms(const char *file_addr, bool is_pcm,
    int bits_per_sample, int src_sample_rate_in_Hz, int src_nb_channels)
{
    if (!file_addr) return -1;

    if (is_pcm) {
        if (src_sample_rate_in_Hz <= 0 || src_nb_channels <= 0) return -1;
        if (bits_per_sample != 8 && bits_per_sample != 16) return -1;
        return get_pcm_file_duration_ms(file_addr, bits_per_sample,
            src_sample_rate_in_Hz, src_nb_channels);
    } else {
        return get_audio_file_duration_ms(file_addr);
    }
}

