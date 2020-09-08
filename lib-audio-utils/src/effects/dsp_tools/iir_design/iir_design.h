//
// Created by layne on 19-4-28.
//

#ifndef AUDIO_EFFECT_IIR_DESIGN_H_
#define AUDIO_EFFECT_IIR_DESIGN_H_
#include <stddef.h>

typedef struct BandT {
    float coeffs[5];
    float states[4];
} Band;

int iir_1st_coeffs_lowpass(Band *self, size_t sample_rate, float freq);
int iir_1st_coeffs_highpass(Band *self, size_t sample_rate, float freq);
int iir_1st_coeffs_allpass(Band *self, size_t sample_rate, float freq);
int iir_2nd_coeffs_lowpass(Band *self, size_t sample_rate, float freq, float q);
int iir_2nd_coeffs_highpass(Band *self, size_t sample_rate, float freq,
                            float q);
int iir_2nd_coeffs_bandpass(Band *self, size_t sample_rate, float freq,
                            float q);
int iir_2nd_coeffs_bandstop(Band *self, size_t sample_rate, float freq,
                            float q);
int iir_2nd_coeffs_allpass(Band *self, size_t sample_rate, float freq, float q);
int iir_2nd_coeffs_low_shelf(Band *self, size_t sample_rate, float freq,
                             float q, float gain);
int iir_2nd_coeffs_high_shelf(Band *self, size_t sample_rate, float freq,
                              float q, float gain);
int iir_2nd_coeffs_peak(Band *self, size_t sample_rate, float freq, float q,
                        float gain);
int iir_1st_coeffs_butterworth_lowpass(Band *self, size_t sample_rate,
                                       float freq);
int iir_1st_coeffs_butterworth_highpass(Band *self, size_t sample_rate,
                                        float freq);
int iir_2nd_coeffs_butterworth_lowpass(Band *self, size_t sample_rate,
                                       float freq);
int iir_2nd_coeffs_butterworth_highpass(Band *self, size_t sample_rate,
                                        float freq);
int iir_2nd_coeffs_butterworth_bandpass(Band *self, size_t sample_rate,
                                        float freq, float q);
int iir_2nd_coeffs_butterworth_bandstop(Band *self, size_t sample_rate,
                                        float freq, float q);
void band_process(Band *self, float *buffer, size_t buffer_size);

#endif  // AUDIO_EFFECT_IIR_DESIGN_H_
