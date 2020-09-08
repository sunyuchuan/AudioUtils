//
// Created by layne on 19-4-28.
//

#include "iir_design.h"
#include <math.h>
#include "error_def.h"

#define SAMPLE_RATE 44100
#define PI 3.14159265358979323f

#define CHECK_SAMPLE_RATE(sr)                                                  \
    do {                                                                       \
        if ((sr) != 8000 && (sr) != 11025 && (sr) != 16000 && (sr) != 22050 && \
            (sr) != 32000 && (sr) != 44100 && (sr) != 48000 &&                 \
            (sr) != 64000 && (sr) != 88200 && (sr) != 96000 &&                 \
            (sr) != 128000 && (sr) != 176400 && (sr) != 192000) {              \
            return AEERROR_INVALID_PARAMETER;                                  \
        }                                                                      \
    } while (0)

int iir_1st_coeffs_lowpass(Band *self, size_t sample_rate, float freq) {
    CHECK_SAMPLE_RATE(sample_rate);
    if (freq <= 0 || freq > sample_rate / 2) {
        return AEERROR_INVALID_PARAMETER;
    }
    float k = tanf(PI * freq / sample_rate);
    self->coeffs[0] = k / (k + 1);
    self->coeffs[1] = k / (k + 1);
    self->coeffs[3] = (k - 1) / (k + 1);
    return 0;
}

int iir_1st_coeffs_highpass(Band *self, size_t sample_rate, float freq) {
    CHECK_SAMPLE_RATE(sample_rate);
    if (freq <= 0 || freq > sample_rate / 2) {
        return AEERROR_INVALID_PARAMETER;
    }
    float k = tanf(PI * freq / sample_rate);
    self->coeffs[0] = 1 / (k + 1);
    self->coeffs[1] = -1 / (k + 1);
    self->coeffs[3] = (k - 1) / (k + 1);
    return 0;
}

int iir_1st_coeffs_allpass(Band *self, size_t sample_rate, float freq) {
    CHECK_SAMPLE_RATE(sample_rate);
    if (freq <= 0 || freq > sample_rate / 2) {
        return AEERROR_INVALID_PARAMETER;
    }
    float k = tanf(PI * freq / sample_rate);
    self->coeffs[0] = (k - 1) / (k + 1);
    self->coeffs[1] = 1.0f;
    self->coeffs[3] = (k - 1) / (k + 1);
    return 0;
}

int iir_2nd_coeffs_lowpass(Band *self, size_t sample_rate, float freq,
                           float q) {
    CHECK_SAMPLE_RATE(sample_rate);
    if (freq <= 0 || freq > sample_rate / 2) {
        return AEERROR_INVALID_PARAMETER;
    }
    float k = tanf(PI * freq / sample_rate);
    float a0 = k * k * q + k + q;
    self->coeffs[0] = k * k * q / a0;
    self->coeffs[1] = 2 * k * k * q / a0;
    self->coeffs[2] = k * k * q / a0;
    self->coeffs[3] = 2 * q * (k * k - 1) / a0;
    self->coeffs[4] = (k * k * q - k + q) / a0;
    return 0;
}

int iir_2nd_coeffs_highpass(Band *self, size_t sample_rate, float freq,
                            float q) {
    CHECK_SAMPLE_RATE(sample_rate);
    if (freq <= 0 || freq > sample_rate / 2) {
        return AEERROR_INVALID_PARAMETER;
    }
    float k = tanf(PI * freq / sample_rate);
    float a0 = k * k * q + k + q;
    self->coeffs[0] = q / a0;
    self->coeffs[1] = -2 * q / a0;
    self->coeffs[2] = q / a0;
    self->coeffs[3] = 2 * q * (k * k - 1) / a0;
    self->coeffs[4] = (k * k * q - k + q) / a0;
    return 0;
}

int iir_2nd_coeffs_bandpass(Band *self, size_t sample_rate, float freq,
                            float q) {
    CHECK_SAMPLE_RATE(sample_rate);
    if (freq <= 0 || freq > sample_rate / 2) {
        return AEERROR_INVALID_PARAMETER;
    }
    float k = tanf(PI * freq / sample_rate);
    float a0 = k * k * q + k + q;
    self->coeffs[0] = k / a0;
    self->coeffs[1] = 0;
    self->coeffs[2] = -k / a0;
    self->coeffs[3] = 2 * q * (k * k - 1) / a0;
    self->coeffs[4] = (k * k * q - k + q) / a0;
    return 0;
}

int iir_2nd_coeffs_bandstop(Band *self, size_t sample_rate, float freq,
                            float q) {
    CHECK_SAMPLE_RATE(sample_rate);
    if (freq <= 0 || freq > sample_rate / 2) {
        return AEERROR_INVALID_PARAMETER;
    }
    float k = tanf(PI * freq / sample_rate);
    float a0 = k * k * q + k + q;
    self->coeffs[0] = q * (1 + k * k) / a0;
    self->coeffs[1] = 2 * q * (k * k - 1) / a0;
    self->coeffs[2] = q * (1 + k * k) / a0;
    self->coeffs[3] = 2 * q * (k * k - 1) / a0;
    self->coeffs[4] = (k * k * q - k + q) / a0;
    return 0;
}

int iir_2nd_coeffs_allpass(Band *self, size_t sample_rate, float freq,
                           float q) {
    CHECK_SAMPLE_RATE(sample_rate);
    if (freq <= 0 || freq > sample_rate / 2) {
        return AEERROR_INVALID_PARAMETER;
    }
    float k = tanf(PI * freq / sample_rate);
    float a0 = k * k * q + k + q;
    self->coeffs[0] = (k * k * q - k + q) / a0;
    self->coeffs[1] = 2 * q * (k * k - 1) / a0;
    self->coeffs[2] = 1;
    self->coeffs[3] = 2 * q * (k * k - 1) / a0;
    self->coeffs[4] = (k * k * q - k + q) / a0;
    return 0;
}

int iir_2nd_coeffs_low_shelf(Band *self, size_t sample_rate, float freq,
                             float q, float gain) {
    CHECK_SAMPLE_RATE(sample_rate);
    if (freq <= 0 || freq > sample_rate / 2) {
        return AEERROR_INVALID_PARAMETER;
    }
    float k = tanf(PI * freq / sample_rate);
    if (gain >= 1.0f) {
        float a0 = 1 + k / q + k * k;
        self->coeffs[0] = (1 + sqrtf(gain) * k / q + gain * k * k) / a0;
        self->coeffs[1] = 2 * (gain * k * k - 1) / a0;
        self->coeffs[2] = (1 - sqrtf(gain) * k / q + gain * k * k) / a0;
        self->coeffs[3] = 2 * (k * k - 1) / a0;
        self->coeffs[4] = (1 - k / q + k * k) / a0;
    } else {
        float a0 = gain + sqrtf(gain) * k / q;
        self->coeffs[0] = gain * (1 + k / q + k * k) / a0;
        self->coeffs[1] = 2 * gain * (k * k - 1) / a0;
        self->coeffs[2] = gain * (1 - k / q + k * k) / a0;
        self->coeffs[3] = 2 * (k * k - gain) / a0;
        self->coeffs[4] = (gain - sqrtf(gain) * k / q + k * k) / a0;
    }
    return 0;
}

int iir_2nd_coeffs_high_shelf(Band *self, size_t sample_rate, float freq,
                              float q, float gain) {
    CHECK_SAMPLE_RATE(sample_rate);
    if (freq <= 0 || freq > sample_rate / 2) {
        return AEERROR_INVALID_PARAMETER;
    }
    float k = tanf(PI * freq / sample_rate);
    if (gain >= 1.0f) {
        float a0 = 1 + k / q + k * k;
        self->coeffs[0] = (gain + sqrtf(gain) * k / q + k * k) / a0;
        self->coeffs[1] = 2 * (k * k - gain) / a0;
        self->coeffs[2] = (gain - sqrtf(gain) * k / q + k * k) / a0;
        self->coeffs[3] = 2 * (k * k - 1) / a0;
        self->coeffs[4] = (1 - k / q + k * k) / a0;
    } else {
        float a0 = 1 + sqrtf(gain) * k / q + gain * k * k;
        self->coeffs[0] = gain * (1 + k / q + k * k) / a0;
        self->coeffs[1] = 2 * gain * (k * k - 1) / a0;
        self->coeffs[2] = gain * (1 - sqrtf(2) * k + k * k) / a0;
        self->coeffs[3] = 2 * (gain * k * k - 1) / a0;
        self->coeffs[4] = (1 - sqrtf(2 * gain) * k + gain * k * k) / a0;
    }
    return 0;
}

int iir_2nd_coeffs_peak(Band *self, size_t sample_rate, float freq, float q,
                        float gain) {
    CHECK_SAMPLE_RATE(sample_rate);
    if (freq <= 0 || freq > sample_rate / 2) {
        return AEERROR_INVALID_PARAMETER;
    }
    float k = tanf(PI * freq / sample_rate);
    if (gain >= 1.0f) {
        float a0 = 1 + k / q + k * k;
        self->coeffs[0] = (1 + gain * k / q + k * k) / a0;
        self->coeffs[1] = 2 * (k * k - 1) / a0;
        self->coeffs[2] = (1 - gain * k / q + k * k) / a0;
        self->coeffs[3] = 2 * (k * k - 1) / a0;
        self->coeffs[4] = (1 - k / q + k * k) / a0;
    } else {
        float a0 = 1 + k / (gain * q) + k * k;
        self->coeffs[0] = (1 + k / q + k * k) / a0;
        self->coeffs[1] = 2 * (k * k - 1) / a0;
        self->coeffs[2] = (1 - k / q + k * k) / a0;
        self->coeffs[3] = 2 * (k * k - 1) / a0;
        self->coeffs[4] = (1 - k / (gain * q) + k * k) / a0;
    }
    return 0;
}

static int iir_1st_coeffs_bilinear_lowpass(Band *self, size_t sample_rate,
                                           float freq, float b0, float b1,
                                           float a0, float a1) {
    CHECK_SAMPLE_RATE(sample_rate);
    if (freq <= 0 || freq > sample_rate / 2) {
        return AEERROR_INVALID_PARAMETER;
    }
    float k;
    if (freq * 2 == sample_rate) {
        k = 0;
    } else {
        k = 2 * PI * freq / tanf(PI * freq / sample_rate);
    }
    float w0 = 2 * PI * freq;
    float z_a0 = k * a0 + a1 * w0;
    self->coeffs[0] = (k * b0 + b1 * w0) / z_a0;
    self->coeffs[1] = (-k * b0 + b1 * w0) / z_a0;
    self->coeffs[3] = (-k * a0 + a1 * w0) / z_a0;
    return 0;
}

static int iir_1st_coeffs_bilinear_highpass(Band *self, size_t sample_rate,
                                            float freq, float b0, float b1,
                                            float a0, float a1) {
    CHECK_SAMPLE_RATE(sample_rate);
    if (freq <= 0 || freq > sample_rate / 2) {
        return AEERROR_INVALID_PARAMETER;
    }
    float k;
    if (freq * 2 == sample_rate) {
        k = 0;
    } else {
        k = 2 * PI * freq / tanf(PI * freq / sample_rate);
    }
    float w0 = 2 * PI * freq;
    float z_a0 = k * a1 + a0 * w0;
    self->coeffs[0] = (k * b1 + b0 * w0) / z_a0;
    self->coeffs[1] = (-k * b1 + b0 * w0) / z_a0;
    self->coeffs[3] = (-k * a1 + a0 * w0) / z_a0;
    return 0;
}

static int iir_2nd_coeffs_bilinear_lowpass(Band *self, size_t sample_rate,
                                           float freq, float b0, float b1,
                                           float b2, float a0, float a1,
                                           float a2) {
    CHECK_SAMPLE_RATE(sample_rate);
    if (freq <= 0 || freq > sample_rate / 2) {
        return AEERROR_INVALID_PARAMETER;
    }
    float k;
    if (freq * 2 == sample_rate) {
        k = 0;
    } else {
        k = 2 * PI * freq / tanf(PI * freq / sample_rate);
    }
    float w0 = 2 * PI * freq;
    float z_a0 = k * k * a0 + k * a1 * w0 + a2 * w0 * w0;
    self->coeffs[0] = (k * k * b0 + k * b1 * w0 + b2 * w0 * w0) / z_a0;
    self->coeffs[1] = (-2 * k * k * b0 + 2 * b2 * w0 * w0) / z_a0;
    self->coeffs[2] = (k * k * b0 - k * b1 * w0 + b2 * w0 * w0) / z_a0;
    self->coeffs[3] = (-2 * k * k * a0 + 2 * a2 * w0 * w0) / z_a0;
    self->coeffs[4] = (k * k * a0 - k * a1 * w0 + a2 * w0 * w0) / z_a0;
    return 0;
}

static int iir_2nd_coeffs_bilinear_highpass(Band *self, size_t sample_rate,
                                            float freq, float b0, float b1,
                                            float b2, float a0, float a1,
                                            float a2) {
    CHECK_SAMPLE_RATE(sample_rate);
    if (freq <= 0 || freq > sample_rate / 2) {
        return AEERROR_INVALID_PARAMETER;
    }
    float k;
    if (freq * 2 == sample_rate) {
        k = 0;
    } else {
        k = 2 * PI * freq / tanf(PI * freq / sample_rate);
    }
    float w0 = 2 * PI * freq;
    float z_a0 = k * k * a2 + k * a1 * w0 + a0 * w0 * w0;
    self->coeffs[0] = (k * k * b2 + k * b1 * w0 + b0 * w0 * w0) / z_a0;
    self->coeffs[1] = (-2 * k * k * b2 + 2 * b0 * w0 * w0) / z_a0;
    self->coeffs[2] = (k * k * b2 - k * b1 * w0 + b0 * w0 * w0) / z_a0;
    self->coeffs[3] = (-2 * k * k * a2 + 2 * a0 * w0 * w0) / z_a0;
    self->coeffs[4] = (k * k * a2 - k * a1 * w0 + a0 * w0 * w0) / z_a0;
    return 0;
}

static int iir_2nd_coeffs_bilinear_bandpass(Band *self, size_t sample_rate,
                                            float freq, float q, float b0,
                                            float b1, float a0, float a1) {
    CHECK_SAMPLE_RATE(sample_rate);
    if (freq <= 0 || freq > sample_rate / 2) {
        return AEERROR_INVALID_PARAMETER;
    }
    float k;
    if (freq * 2 == sample_rate) {
        k = 0;
    } else {
        k = 2 * PI * freq / tanf(PI * freq / sample_rate);
    }
    float w0 = 2 * PI * freq;
    float z_a0 = k * k * q * a0 + k * a1 * w0 + q * a0 * w0 * w0;
    self->coeffs[0] = (k * k * q * b0 + k * b1 * w0 + q * b0 * w0 * w0) / z_a0;
    self->coeffs[1] = (-2 * k * k * q * b0 + 2 * q * b0 * w0 * w0) / z_a0;
    self->coeffs[2] = (k * k * q * b0 - k * b1 * w0 + q * b0 * w0 * w0) / z_a0;
    self->coeffs[3] = (-2 * k * k * q * a0 + 2 * q * a0 * w0 * w0) / z_a0;
    self->coeffs[4] = (k * k * q * a0 - k * a1 * w0 + q * a0 * w0 * w0) / z_a0;
    return 0;
}

static int iir_2nd_coeffs_bilinear_bandstop(Band *self, size_t sample_rate,
                                            float freq, float q, float b0,
                                            float b1, float a0, float a1) {
    CHECK_SAMPLE_RATE(sample_rate);
    if (freq <= 0 || freq > sample_rate / 2) {
        return AEERROR_INVALID_PARAMETER;
    }
    float k;
    if (freq * 2 == sample_rate) {
        k = 0;
    } else {
        k = 2 * PI * freq / tanf(PI * freq / sample_rate);
    }
    float w0 = 2 * PI * freq;
    float z_a0 = k * k * q * a1 + k * a0 * w0 + q * a1 * w0 * w0;
    self->coeffs[0] = (k * k * q * b1 + k * b0 * w0 + q * b1 * w0 * w0) / z_a0;
    self->coeffs[1] = (-2 * k * k * q * b1 + 2 * q * b1 * w0 * w0) / z_a0;
    self->coeffs[2] = (k * k * q * b1 - k * b0 * w0 + q * b1 * w0 * w0) / z_a0;
    self->coeffs[3] = (-2 * k * k * q * a1 + 2 * q * a1 * w0 * w0) / z_a0;
    self->coeffs[4] = (k * k * q * a1 - k * a0 * w0 + q * a1 * w0 * w0) / z_a0;
    return 0;
}

int iir_1st_coeffs_butterworth_lowpass(Band *self, size_t sample_rate,
                                       float freq) {
    return iir_1st_coeffs_bilinear_lowpass(self, sample_rate, freq, 0, 1.0f,
                                           1.0f, 1.0f);
}

int iir_1st_coeffs_butterworth_highpass(Band *self, size_t sample_rate,
                                        float freq) {
    return iir_1st_coeffs_bilinear_highpass(self, sample_rate, freq, 0, 1.0f,
                                            1.0f, 1.0f);
}

int iir_2nd_coeffs_butterworth_lowpass(Band *self, size_t sample_rate,
                                       float freq) {
    return iir_2nd_coeffs_bilinear_lowpass(self, sample_rate, freq, 0, 0, 1, 1,
                                           sqrtf(2.0f), 1);
}

int iir_2nd_coeffs_butterworth_highpass(Band *self, size_t sample_rate,
                                        float freq) {
    return iir_2nd_coeffs_bilinear_highpass(self, sample_rate, freq, 0, 0, 1, 1,
                                            sqrtf(2.0f), 1);
}

int iir_2nd_coeffs_butterworth_bandpass(Band *self, size_t sample_rate,
                                        float freq, float q) {
    return iir_2nd_coeffs_bilinear_bandpass(self, sample_rate, freq, q, 0, 1, 1,
                                            1);
}

int iir_2nd_coeffs_butterworth_bandstop(Band *self, size_t sample_rate,
                                        float freq, float q) {
    return iir_2nd_coeffs_bilinear_bandstop(self, sample_rate, freq, q, 0, 1, 1,
                                            1);
}

void band_process(Band *self, float *buffer, size_t buffer_size) {
    float b0 = self->coeffs[0];
    float b1 = self->coeffs[1];
    float b2 = self->coeffs[2];
    float a1 = self->coeffs[3];
    float a2 = self->coeffs[4];
    float s1 = self->states[0];
    float s2 = self->states[1];
    float s3 = self->states[2];
    float s4 = self->states[3];

    for (size_t i = 0; i < buffer_size; ++i) {
        float y = b0 * buffer[i] + b1 * s1 + b2 * s2 - a1 * s3 - a2 * s4;
        s2 = s1;
        s1 = buffer[i];
        s4 = s3;
        s3 = y;
        buffer[i] = y;
    }

    self->states[0] = s1;
    self->states[1] = s2;
    self->states[2] = s3;
    self->states[3] = s4;
}