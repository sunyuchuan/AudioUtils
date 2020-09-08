#include "fade_in_out.h"
#include "log.h"

void check_fade_in_out(FadeInOut *fade_io,
        int buffer_start_time, int buffer_duration, int sample_rate,
        int bgm_start_time_ms, int bgm_end_time_ms) {
    if (!fade_io) {
        return;
    }
    FadeInOut *self = fade_io;
    int buffer_end_time = buffer_start_time + buffer_duration;

    if (self->fade_in_time_ms > 0 && buffer_end_time >= bgm_start_time_ms &&
            buffer_start_time < bgm_start_time_ms + self->fade_in_time_ms) {
        self->fade_in = true;
    } else {
        self->fade_in = false;
    }

    if (self->fade_out_time_ms > 0
            && (buffer_end_time >= bgm_end_time_ms - self->fade_out_time_ms)) {
        self->fade_out = true;
        if (buffer_start_time < bgm_end_time_ms - self->fade_out_time_ms) {
            self->fade_out_start_index = sample_rate *
                (float)(bgm_end_time_ms - self->fade_out_time_ms - buffer_start_time) / 1000;
        } else {
            self->fade_out_start_index = 0;
        }
    } else {
        self->fade_out = false;
    }
}

void scale_with_ramp(FadeInOut *fade_io, short *data,
        int nb_samples, int nb_channels) {
    if (!fade_io || !data) {
        return;
    }
    FadeInOut *self = fade_io;

    float rate = 0.0f;
    if (self->fade_in) {
        if (self->fade_in_samples_count > self->fade_in_nb_samples) {
            rate = 1.0f;
        } else {
            rate = (float)self->fade_in_samples_count / (float)self->fade_in_nb_samples;
        }
        for (int i = 0; i < nb_samples; ++i) {
            data[i] *= (rate);
            if (nb_channels == 2) {
                data[nb_samples + i] *= (rate);
            }
            self->fade_in_samples_count ++;
            if (self->fade_in_samples_count > self->fade_in_nb_samples) {
                rate = 1.0f;
            } else {
                rate = (float)self->fade_in_samples_count / (float)self->fade_in_nb_samples;
            }
        }
    } else if (self->fade_out) {
        if (self->fade_out_samples_count > self->fade_out_nb_samples) {
            rate = 0.0f;
        } else {
            rate = (1.0f - (float)self->fade_out_samples_count / (float)self->fade_out_nb_samples);
        }
        for (int i = 0; i < nb_samples; ++i) {
            data[i] *= (rate);
            if (nb_channels == 2) {
                data[nb_samples + i] *= (rate);
            }
            if (i >= self->fade_out_start_index) self->fade_out_samples_count++;
            if (self->fade_out_samples_count > self->fade_out_nb_samples) {
                rate = 0.0f;
            } else {
                rate = (1.0f - (float)self->fade_out_samples_count / (float)self->fade_out_nb_samples);
            }
        }
    }
}

