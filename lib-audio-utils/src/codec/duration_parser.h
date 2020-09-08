#ifndef DURATION_PARSER_H
#define DURATION_PARSER_H

int get_audio_file_duration_ms(const char *file_addr);

int get_pcm_file_duration_ms(const char *file_addr,
    int bits_per_sample, int src_sample_rate_in_Hz, int src_nb_channels);

#endif
