#ifndef XM_DURATION_PARSER_H
#define XM_DURATION_PARSER_H
#include <stdbool.h>

/**
 * @brief Get the duration of an audio file
 *
 * @param file_addr Input audio file path.
 * @param is_pcm true|false, true:Input audio file is a pcm file
 * @param bits_per_sample If it is a pcm file, sampling depth of pcm data
 * @param src_sample_rate_in_Hz If it is a pcm file, sample_rate of pcm data
 * @param src_nb_channels If it is a pcm file, nb_channels of pcm data
 * @return the duration of an audio file.
 */
int get_file_duration_ms(const char *file_addr, bool is_pcm,
    int bits_per_sample, int src_sample_rate_in_Hz, int src_nb_channels);

#endif //XM_DURATION_PARSER_H
