#ifndef I_DECODER_H
#define I_DECODER_H
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#define BITS_PER_SAMPLE_16 16
#define BITS_PER_SAMPLE_8 8
#define PCM_FILE_EOF -7000

#define FLOAT_EPS 1e-6
#define DOUBLE_EPS 1e-15

static inline void set_gain(short *buffer, int buffer_len, short volume_fix) {
    if (!buffer || buffer_len <= 0 || volume_fix == 32767) {
        return;
    }

    for (int i = 0; i < buffer_len; ++i) {
        buffer[i] = buffer[i] * volume_fix >> 15;
    }
}

static inline int calculation_duration_ms(int64_t size,
    float bytes_per_sample, int nb_channles, int sample_rate) {
    if (fabs(bytes_per_sample) <= FLOAT_EPS || nb_channles == 0
            || sample_rate == 0) {
        return 0;
    }
    return 1000 * (size / bytes_per_sample / nb_channles / sample_rate);
}

typedef struct IAudioDecoder_Opaque IAudioDecoder_Opaque;
typedef struct IAudioDecoder
{
    IAudioDecoder_Opaque *opaque;
    int out_sample_rate;
    int out_nb_channels;
    int out_bits_per_sample;
    int duration_ms;

    void (*func_free)(IAudioDecoder_Opaque *opaque);
    int (*func_get_pcm_frame)(IAudioDecoder_Opaque *opaque,
        short *buffer, int buffer_size_in_short, bool loop);
    int (*func_set_crop_pos)(IAudioDecoder_Opaque *opaque,
        int crop_start_time_in_ms, int crop_end_time_in_ms);
    int (*func_seekTo)(IAudioDecoder_Opaque *opaque, int seek_pos_ms);
} IAudioDecoder;

void IAudioDecoder_free(IAudioDecoder *decoder);
void IAudioDecoder_freep(IAudioDecoder **decoder);
int IAudioDecoder_get_pcm_frame(IAudioDecoder *decoder,
    short *buffer, int buffer_size_in_short, bool loop);
int IAudioDecoder_seekTo(IAudioDecoder *decoder, int seek_pos_ms);
int IAudioDecoder_set_crop_pos(IAudioDecoder *decoder,
    int crop_start_time_in_ms, int crop_end_time_in_ms);
IAudioDecoder *IAudioDecoder_create(size_t opaque_size);

#endif // I_DECODER_H
