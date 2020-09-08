#ifndef _FADE_IN_OUT_H_
#define _FADE_IN_OUT_H_
#include <stdbool.h>

typedef struct FadeInOut {
    int fade_in_time_ms;
    int fade_out_time_ms;
    int fade_in_nb_samples;
    int fade_out_nb_samples;
    int fade_in_samples_count;
    int fade_out_samples_count;
    int fade_out_start_index;
    bool fade_in;
    bool fade_out;
} FadeInOut;

void check_fade_in_out(FadeInOut *fade_io,
        int buffer_start_time, int buffer_duration, int sample_rate,
        int bgm_start_time_ms, int bgm_end_time_ms);
void scale_with_ramp(FadeInOut *fade_io, short *data,
        int nb_samples, int nb_channels);
#endif
