#include "compressor.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define FFMIN(a, b) ((a) > (b) ? (b) : (a))
#define FFMIN3(a, b, c) FFMIN(FFMIN(a, b), c)

struct CompressorT {
    float output_gain;
    float attack_time;
    float decay_time;
    float average_time;
    float compressor_slopes;
    float compressor_threshold_in_dB;
    float expander_slopes;
    float expander_threshold_in_dB;
    float xrms;
    float gain;
    float delay_in_sec;  /* Delay to apply before companding */
    float* delay_buf;    /* Old samples, used for delay processing */
    int delay_buf_size;  /* Size of delay_buf in samples */
    int delay_buf_index; /* Index into delay_buf */
    int delay_buf_cnt;   /* No. of active entries in delay_buf */
    int sample_rate;
};

Compressor* CompressorCreate(const int sample_rate) {
    Compressor* self = (Compressor*)calloc(1, sizeof(Compressor));
    if (NULL == self) return NULL;

    self->sample_rate = sample_rate;
    self->output_gain = 1.0f;
    self->xrms = 0.0f;
    self->gain = 1.0f;
    self->average_time = 0.01;
    self->compressor_threshold_in_dB = 0.0f;
    self->compressor_slopes = 1.0f;
    self->expander_threshold_in_dB = -90.0f;
    self->expander_slopes = -1.0f;
    self->delay_in_sec = 0.001f;
    self->delay_buf_size = self->delay_in_sec * self->sample_rate;
    self->delay_buf = (float*)calloc(self->delay_buf_size, sizeof(float));
    if (NULL == self->delay_buf) CompressorFree(&self);

    return self;
}

void CompressorFree(Compressor** inst) {
    if (NULL == inst || NULL == *inst) return;
    Compressor* self = *inst;

    if (self->delay_buf) {
        free(self->delay_buf);
        self->delay_buf = NULL;
    }
    free(*inst);
    *inst = NULL;
}

void CompressorSet(Compressor* inst, const float compressor_threshold_in_dB,
                   const float ratio, const float attack_time_in_ms,
                   const float decay_time_in_ms,
                   const float output_gain_in_dB) {
    // TODO: 验证为什么rms计算的幅度与原始幅度相差3dB
    inst->compressor_threshold_in_dB = compressor_threshold_in_dB;
    inst->compressor_slopes = 1.0f - 1.0f / ratio;
    inst->attack_time =
        1.0f - expf(-2.2f * 1000.0f / inst->sample_rate / attack_time_in_ms);
    inst->decay_time =
        1.0f - expf(-2.2f * 1000.0f / inst->sample_rate / decay_time_in_ms);
    inst->output_gain = powf(10.0f, output_gain_in_dB / 20);
    float average_time_in_ms = 100.0f;
    inst->average_time = 1.0f - expf(-2.2f * 1000.0f / inst->sample_rate / average_time_in_ms);
}

int CompressorProcess(Compressor* inst, float* buffer, const int buffer_size) {
    if (NULL == inst || fabs(inst->compressor_slopes - 0.0f) <= 0.001)
        return buffer_size;
    int nb_samples = 0;

    for (int i = 0; i < buffer_size; ++i) {
        inst->xrms = (1.0f - inst->average_time) * inst->xrms +
                     inst->average_time * buffer[i] * buffer[i];
        float X = 10.0f * log10f(inst->xrms);
        // float G = FFMIN(0, inst->compressor_slopes *
        //                        (inst->compressor_threshold_in_dB - X));
        // printf("xrms = %f X = %f\n", inst->xrms, X);
        float G = FFMIN3(
            0, inst->compressor_slopes * (inst->compressor_threshold_in_dB - X),
            inst->expander_slopes * (inst->expander_threshold_in_dB - X));
        float f = powf(10.0f, G / 20.0f);
        float coeff = f < inst->gain ? inst->attack_time : inst->decay_time;
        inst->gain = (1.0f - coeff) * inst->gain + coeff * f;

        if (inst->delay_buf_size <= 0) {
            buffer[nb_samples++] *= inst->gain * inst->output_gain;
        } else {
            float tmp = buffer[i];
            if (inst->delay_buf_cnt < inst->delay_buf_size) {
                inst->delay_buf_cnt++;
            } else {
                buffer[nb_samples++] = inst->delay_buf[inst->delay_buf_index] *
                                       inst->gain * inst->output_gain;
            }
            inst->delay_buf[inst->delay_buf_index++] = tmp;
            inst->delay_buf_index %= inst->delay_buf_size;
        }
    }
    return nb_samples;
}