#ifndef XM_WAV_UTILS_H
#define XM_WAV_UTILS_H
#include <stdbool.h>

/**
 * @brief concat wav file
 *
 * @param in_wav_path Input wav file path array
 * @param nb_in_wav number of Input wav file
 * @param out_wav_path output wav file path
 * @return false means failure, true means success.
 */
bool xm_wav_utils_concat(char * const *in_wav_path,
    int nb_in_wav, const char *out_wav_path);

/**
 * @brief crop wav file
 *
 * @param in_wav_path Input wav file path
 * @param crop_start_ms start time in ms
 * @param crop_end_ms end time in ms
 * @param out_wav_path output wav file path
 * @return false means failure, true means success.
 */
bool xm_wav_utils_crop(const char *in_wav_path,
    long crop_start_ms, long crop_end_ms, const char *out_wav_path);

/**
 * @brief get wav audio duration
 *
 * @param in_wav_path Input wav file path
 * @return duration in ms
 */
long xm_wav_utils_get_duration(const char *in_wav_path);

#endif // XM_WAV_UTILS_H