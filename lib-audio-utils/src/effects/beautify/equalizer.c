#include "equalizer.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "dsp_tools/iir_design/iir_design.h"

#define FFMAX(a, b) ((a) > (b) ? (a) : (b))
#define FFMIN(a, b) ((a) > (b) ? (b) : (a))

#define PI 3.14159265358979323f

struct EqualizerT {
    Band* equalizer_bands;
    short nb_equalizer_bands;
    int sample_rate;
};

static void CreateCleanVoice(Equalizer* inst) {
    inst->nb_equalizer_bands = 4;
    inst->equalizer_bands =
        (Band*)calloc(inst->nb_equalizer_bands, sizeof(Band));
    if (NULL == inst->equalizer_bands) return;

    iir_2nd_coeffs_butterworth_highpass(inst->equalizer_bands,
                                        inst->sample_rate, 50.0f);
    iir_2nd_coeffs_peak(inst->equalizer_bands + 1, inst->sample_rate, 200.f,
                        2.0f, 1.66f);
    iir_2nd_coeffs_peak(inst->equalizer_bands + 2, inst->sample_rate, 80.f,
                        2.0f, 1.43f);
    iir_2nd_coeffs_high_shelf(inst->equalizer_bands + 3, inst->sample_rate,
                              4300.0f, 0.7f, 1.74f);
}

static void CreateBassEffect(Equalizer* inst) {
    inst->nb_equalizer_bands = 3;
    inst->equalizer_bands =
        (Band*)calloc(inst->nb_equalizer_bands, sizeof(Band));
    if (NULL == inst->equalizer_bands) return;

    iir_2nd_coeffs_butterworth_highpass(inst->equalizer_bands,
                                        inst->sample_rate, 50.0f);
    iir_2nd_coeffs_peak(inst->equalizer_bands + 1, inst->sample_rate, 150.f,
                        2.0f, 1.95f);
    iir_2nd_coeffs_peak(inst->equalizer_bands + 2, inst->sample_rate, 500.0f,
                        2.0f, 1.74f);
}

static void CreateLowVoice(Equalizer* inst) {
    inst->nb_equalizer_bands = 4;
    inst->equalizer_bands =
        (Band*)calloc(inst->nb_equalizer_bands, sizeof(Band));
    if (NULL == inst->equalizer_bands) return;

    iir_2nd_coeffs_butterworth_highpass(inst->equalizer_bands,
                                        inst->sample_rate, 50.0f);
    iir_2nd_coeffs_peak(inst->equalizer_bands + 1, inst->sample_rate, 200.f,
                        2.0f, 1.53f);
    iir_2nd_coeffs_peak(inst->equalizer_bands + 2, inst->sample_rate, 1250.0f,
                        2.0f, 0.813f);
    iir_2nd_coeffs_high_shelf(inst->equalizer_bands + 3, inst->sample_rate,
                              7500.0f, 0.7f, 0.708f);
}

static void CreatePenetratingEffect(Equalizer* inst) {
    inst->nb_equalizer_bands = 2;
    inst->equalizer_bands =
        (Band*)calloc(inst->nb_equalizer_bands, sizeof(Band));
    if (NULL == inst->equalizer_bands) return;

    iir_2nd_coeffs_peak(inst->equalizer_bands, inst->sample_rate, 2000.f, 2.0f,
                        1.51f);
    iir_2nd_coeffs_high_shelf(inst->equalizer_bands + 1, inst->sample_rate,
                              4300.0f, 0.7f, 1.74f);
}

static void CreateMagneticEffect(Equalizer* inst) {
    inst->nb_equalizer_bands = 3;
    inst->equalizer_bands =
        (Band*)calloc(inst->nb_equalizer_bands, sizeof(Band));
    if (NULL == inst->equalizer_bands) return;

    iir_2nd_coeffs_butterworth_highpass(inst->equalizer_bands,
                                        inst->sample_rate, 50.0f);
    iir_2nd_coeffs_peak(inst->equalizer_bands + 1, inst->sample_rate, 500.f,
                        2.0f, 1.74f);
    iir_2nd_coeffs_peak(inst->equalizer_bands + 2, inst->sample_rate, 1200.0f,
                        2.0f, 1.55f);
}

static void CreateSoftPitch(Equalizer* inst) {
    inst->nb_equalizer_bands = 5;
    inst->equalizer_bands =
        (Band*)calloc(inst->nb_equalizer_bands, sizeof(Band));
    if (NULL == inst->equalizer_bands) return;

    iir_2nd_coeffs_peak(inst->equalizer_bands, inst->sample_rate, 300.f, 2.0f,
                        1.2f);
    iir_2nd_coeffs_peak(inst->equalizer_bands + 1, inst->sample_rate, 1200.f,
                        2.0f, 1.82f);
    iir_2nd_coeffs_peak(inst->equalizer_bands + 2, inst->sample_rate, 5000.f,
                        4.0f, 0.501f);  // -6dB
    iir_2nd_coeffs_peak(inst->equalizer_bands + 3, inst->sample_rate, 8000.f,
                        4.0f, 0.501f);  // -6dB
    iir_2nd_coeffs_high_shelf(inst->equalizer_bands + 4, inst->sample_rate,
                              10000.0f, 0.7f, 0.562f);  //-5db
}

Equalizer* EqualizerCreate(const int sample_rate) {
    Equalizer* self = (Equalizer*)calloc(1, sizeof(Equalizer));
    if (self) self->sample_rate = sample_rate;
    return self;
}

void EqualizerFree(Equalizer** inst) {
    if (NULL == inst || NULL == *inst) return;
    Equalizer* self = *inst;
    if (self->equalizer_bands) {
        free(self->equalizer_bands);
        self->equalizer_bands = NULL;
    }
    free(*inst);
    *inst = NULL;
}

void EqualizerSetMode(Equalizer* inst, const enum EqualizerMode mode) {
    if (inst->equalizer_bands) {
        free(inst->equalizer_bands);
        inst->equalizer_bands = NULL;
        inst->nb_equalizer_bands = 0;
    }

    switch (mode) {
        case EqCleanVoice:
            // 清晰人声
            CreateCleanVoice(inst);
            break;
        case EqBass:
            // 低音 -> 沉稳
            CreateBassEffect(inst);
            break;
        case EqLowVoice:
            // 低沉 -> 低音
            CreateLowVoice(inst);
            break;
        case EqPenetrating:
            // 穿透
            CreatePenetratingEffect(inst);
            break;
        case EqMagnetic:
            // 磁性
            CreateMagneticEffect(inst);
            break;
        case EqSoftPitch:
            // 柔和高音
            CreateSoftPitch(inst);
            break;
        default:
            break;
    }
}

void EqualizerProcess(Equalizer* inst, float* buffer, const int buffer_size) {
    if (NULL == inst) return;

    for (int i = 0; i < inst->nb_equalizer_bands; ++i) {
        band_process(inst->equalizer_bands + i, buffer, buffer_size);
    }
}