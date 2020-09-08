#include "multiband_compressor.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compressor.h"
#include "dsp_tools/iir_design/iir_design.h"

typedef struct CompressorBandT {
    float* buffer;
    Band* iir_band;
    Compressor* compressor;
} CompressorBand;

struct MulCompressorT {
    int sample_rate;
    int max_nb_samples;
    short nb_compressor_bands;
    CompressorBand* compressor_bands;
};

MulCompressor* MulCompressorCreate(const int sample_rate) {
    MulCompressor* self = (MulCompressor*)calloc(1, sizeof(MulCompressor));
    if (NULL == self) return NULL;

    self->sample_rate = sample_rate;
    return self;
}

static void CompressorBandFree(MulCompressor* inst) {
    if (NULL == inst || NULL == inst->compressor_bands) return;
    for (int i = 0; i < inst->nb_compressor_bands; ++i) {
        if (inst->compressor_bands + i) {
            if ((inst->compressor_bands + i)->buffer) {
                free((inst->compressor_bands + i)->buffer);
                (inst->compressor_bands + i)->buffer = NULL;
            }
            if ((inst->compressor_bands + i)->iir_band) {
                free((inst->compressor_bands + i)->iir_band);
                (inst->compressor_bands + i)->iir_band = NULL;
            }
            if ((inst->compressor_bands + i)->compressor)
                CompressorFree(&(inst->compressor_bands + i)->compressor);
        }
    }

    if (inst->compressor_bands) {
        free(inst->compressor_bands);
        inst->compressor_bands = NULL;
    }
    inst->nb_compressor_bands = 0;
    inst->max_nb_samples = 0;
}

void MulCompressorFree(MulCompressor** inst) {
    if (NULL == inst || NULL == *inst) return;
    MulCompressor* self = *inst;

    CompressorBandFree(self);

    free(*inst);
    *inst = NULL;
}

static void CreateCleanVoice(MulCompressor* inst) {
    inst->nb_compressor_bands = 4;
    inst->compressor_bands = (CompressorBand*)calloc(inst->nb_compressor_bands,
                                                     sizeof(CompressorBand));
    if (NULL == inst->compressor_bands) return;

    // 设置第一个压缩器
    inst->compressor_bands->iir_band = (Band*)calloc(1, sizeof(Band));
    if (NULL == inst->compressor_bands->iir_band) goto end;
    iir_2nd_coeffs_butterworth_lowpass(inst->compressor_bands->iir_band,
                                       inst->sample_rate, 150);
    inst->compressor_bands->compressor = CompressorCreate(inst->sample_rate);
    CompressorSet(inst->compressor_bands->compressor, -15.0f, 2.0f, 1.0f, 50.0f,
                  -3.0f);

    // 设置第二个压缩器
    (inst->compressor_bands + 1)->iir_band = (Band*)calloc(1, sizeof(Band));
    if (NULL == (inst->compressor_bands + 1)->iir_band) goto end;
    iir_2nd_coeffs_butterworth_bandpass(
        (inst->compressor_bands + 1)->iir_band, inst->sample_rate,
        sqrt(150.0f * 3360.0f), sqrt(150.0f * 3360.0f) / (3360.0f - 150.0f));
    (inst->compressor_bands + 1)->compressor =
        CompressorCreate(inst->sample_rate);
    CompressorSet((inst->compressor_bands + 1)->compressor, -18, 2.0f, 1.0f,
                  50.0f, -2.7f);

    // 设置第三个压缩器
    (inst->compressor_bands + 2)->iir_band = (Band*)calloc(1, sizeof(Band));
    if (NULL == (inst->compressor_bands + 2)->iir_band) goto end;
    iir_2nd_coeffs_butterworth_bandpass(
        (inst->compressor_bands + 2)->iir_band, inst->sample_rate,
        sqrt(3360.0f * 9150.0f), sqrt(3360.0f * 9150.0f) / (9150.0f - 3360.0f));
    (inst->compressor_bands + 2)->compressor =
        CompressorCreate(inst->sample_rate);
    CompressorSet((inst->compressor_bands + 2)->compressor, -15, 2.0f, 1.0f,
                  50.0f, 4.0f);

    // 设置第四个压缩器
    (inst->compressor_bands + 3)->iir_band = (Band*)calloc(1, sizeof(Band));
    if (NULL == (inst->compressor_bands + 3)->iir_band) goto end;
    iir_2nd_coeffs_butterworth_highpass(inst->compressor_bands->iir_band,
                                        inst->sample_rate, 9150.0f);
    (inst->compressor_bands + 3)->compressor =
        CompressorCreate(inst->sample_rate);
    CompressorSet((inst->compressor_bands + 3)->compressor, -15.0f, 2.0f, 1.0f,
                  50.0f, 1.0f);

    return;
end:
    CompressorBandFree(inst);
}

static void CreateBassEffect(MulCompressor* inst) {
    inst->nb_compressor_bands = 4;
    inst->compressor_bands = (CompressorBand*)calloc(inst->nb_compressor_bands,
                                                     sizeof(CompressorBand));
    if (NULL == inst->compressor_bands) return;

    // 设置第一个压缩器
    inst->compressor_bands->iir_band = (Band*)calloc(1, sizeof(Band));
    if (NULL == inst->compressor_bands->iir_band) goto end;
    iir_2nd_coeffs_butterworth_lowpass(inst->compressor_bands->iir_band,
                                       inst->sample_rate, 195.2f);
    inst->compressor_bands->compressor = CompressorCreate(inst->sample_rate);
    CompressorSet(inst->compressor_bands->compressor, -15.0f, 2.0f, 1.0f, 50.0f,
                  -0.5f);

    // 设置第二个压缩器
    (inst->compressor_bands + 1)->iir_band = (Band*)calloc(1, sizeof(Band));
    if (NULL == (inst->compressor_bands + 1)->iir_band) goto end;
    iir_2nd_coeffs_butterworth_bandpass(
        (inst->compressor_bands + 1)->iir_band, inst->sample_rate,
        sqrt(195.2f * 527.7f), sqrt(195.2 * 527.7f) / (527.7f - 195.2f));
    (inst->compressor_bands + 1)->compressor =
        CompressorCreate(inst->sample_rate);
    CompressorSet((inst->compressor_bands + 1)->compressor, -15.0f, 2.0f, 1.0f,
                  50.0f, 4.0f);

    // 设置第三个压缩器
    (inst->compressor_bands + 2)->iir_band = (Band*)calloc(1, sizeof(Band));
    if (NULL == (inst->compressor_bands + 2)->iir_band) goto end;
    iir_2nd_coeffs_butterworth_bandpass(
        (inst->compressor_bands + 2)->iir_band, inst->sample_rate,
        sqrt(527.7f * 11250.0f),
        sqrt(527.37f * 11250.0f) / (11250.0f - 527.7f));
    (inst->compressor_bands + 2)->compressor =
        CompressorCreate(inst->sample_rate);
    CompressorSet((inst->compressor_bands + 2)->compressor, -15.0f, 4.5f, 1.0f,
                  50.0f, -2.0f);

    // 设置第四个压缩器
    (inst->compressor_bands + 3)->iir_band = (Band*)calloc(1, sizeof(Band));
    if (NULL == (inst->compressor_bands + 3)->iir_band) goto end;
    iir_2nd_coeffs_butterworth_highpass(inst->compressor_bands->iir_band,
                                        inst->sample_rate, 11250.0f);
    (inst->compressor_bands + 3)->compressor =
        CompressorCreate(inst->sample_rate);
    CompressorSet((inst->compressor_bands + 3)->compressor, -15.0f, 2.0f, 1.0f,
                  50.0f, -3.0f);
    return;
end:
    CompressorBandFree(inst);
}

static void CreateLowVoice(MulCompressor* inst) {
    inst->nb_compressor_bands = 4;
    inst->compressor_bands = (CompressorBand*)calloc(inst->nb_compressor_bands,
                                                     sizeof(CompressorBand));
    if (NULL == inst->compressor_bands) return;

    // 设置第一个压缩器
    inst->compressor_bands->iir_band = (Band*)calloc(1, sizeof(Band));
    if (NULL == inst->compressor_bands->iir_band) goto end;
    iir_2nd_coeffs_butterworth_lowpass(inst->compressor_bands->iir_band,
                                       inst->sample_rate, 400.0f);
    inst->compressor_bands->compressor = CompressorCreate(inst->sample_rate);
    CompressorSet(inst->compressor_bands->compressor, -15.0f, 2.0f, 1.0f, 50.0f,
                  2.0f);

    // 设置第二个压缩器
    (inst->compressor_bands + 1)->iir_band = (Band*)calloc(1, sizeof(Band));
    if (NULL == (inst->compressor_bands + 1)->iir_band) goto end;
    iir_2nd_coeffs_butterworth_bandpass(
        (inst->compressor_bands + 1)->iir_band, inst->sample_rate,
        sqrt(150.0f * 3360.0f), sqrt(150.0f * 3360.0) / (3360.f - 150.0f));
    (inst->compressor_bands + 1)->compressor =
        CompressorCreate(inst->sample_rate);
    CompressorSet((inst->compressor_bands + 1)->compressor, -15.3f, 2.0f, 1.0f,
                  50.0f, 3.0f);

    // 设置第三个压缩器
    (inst->compressor_bands + 2)->iir_band = (Band*)calloc(1, sizeof(Band));
    if (NULL == (inst->compressor_bands + 2)->iir_band) goto end;
    iir_2nd_coeffs_butterworth_bandpass(
        (inst->compressor_bands + 2)->iir_band, inst->sample_rate,
        sqrt(3360.0f * 9150.0f), sqrt(3360.0f * 9150.0f) / (9150.0f - 3360.0f));
    (inst->compressor_bands + 2)->compressor =
        CompressorCreate(inst->sample_rate);
    CompressorSet((inst->compressor_bands + 2)->compressor, -20.0f, 2.0f, 1.0f,
                  50.0f, 2.0f);

    // 设置第四个压缩器
    (inst->compressor_bands + 3)->iir_band = (Band*)calloc(1, sizeof(Band));
    if (NULL == (inst->compressor_bands + 3)->iir_band) goto end;
    iir_2nd_coeffs_butterworth_highpass(inst->compressor_bands->iir_band,
                                        inst->sample_rate, 9150.0f);
    (inst->compressor_bands + 3)->compressor =
        CompressorCreate(inst->sample_rate);
    CompressorSet((inst->compressor_bands + 3)->compressor, -20.0f, 2.0f, 1.0f,
                  50.0f, 2.0f);
    return;
end:
    CompressorBandFree(inst);
}

static void CreatePenetratingEffect(MulCompressor* inst) {
    inst->nb_compressor_bands = 4;
    inst->compressor_bands = (CompressorBand*)calloc(inst->nb_compressor_bands,
                                                     sizeof(CompressorBand));
    if (NULL == inst->compressor_bands) return;

    // 设置第一个压缩器
    inst->compressor_bands->iir_band = (Band*)calloc(1, sizeof(Band));
    if (NULL == inst->compressor_bands->iir_band) goto end;
    iir_2nd_coeffs_butterworth_lowpass(inst->compressor_bands->iir_band,
                                       inst->sample_rate, 150.0f);
    inst->compressor_bands->compressor = CompressorCreate(inst->sample_rate);
    CompressorSet(inst->compressor_bands->compressor, -15.0f, 2.0f, 1.0f, 50.0f,
                  -5.0f);

    // 设置第二个压缩器
    (inst->compressor_bands + 1)->iir_band = (Band*)calloc(1, sizeof(Band));
    if (NULL == (inst->compressor_bands + 1)->iir_band) goto end;
    iir_2nd_coeffs_butterworth_bandpass(
        (inst->compressor_bands + 1)->iir_band, inst->sample_rate,
        sqrt(150.0f * 1020.0f), sqrt(150.0f * 1020.0) / (1020.f - 150.0f));
    (inst->compressor_bands + 1)->compressor =
        CompressorCreate(inst->sample_rate);
    CompressorSet((inst->compressor_bands + 1)->compressor, -15.0f, 2.0f, 1.0f,
                  50.0f, 0.0f);

    // 设置第三个压缩器
    (inst->compressor_bands + 2)->iir_band = (Band*)calloc(1, sizeof(Band));
    if (NULL == (inst->compressor_bands + 2)->iir_band) goto end;
    iir_2nd_coeffs_butterworth_bandpass(
        (inst->compressor_bands + 2)->iir_band, inst->sample_rate,
        sqrt(1020.0f * 5040.0f), sqrt(1020.0f * 5040.0f) / (5040.0f - 1020.0f));
    (inst->compressor_bands + 2)->compressor =
        CompressorCreate(inst->sample_rate);
    CompressorSet((inst->compressor_bands + 2)->compressor, -15.0f, 2.0f, 1.0f,
                  50.0f, 2.0f);

    // 设置第四个压缩器
    (inst->compressor_bands + 3)->iir_band = (Band*)calloc(1, sizeof(Band));
    if (NULL == (inst->compressor_bands + 3)->iir_band) goto end;
    iir_2nd_coeffs_butterworth_highpass(inst->compressor_bands->iir_band,
                                        inst->sample_rate, 5040.0f);
    (inst->compressor_bands + 3)->compressor =
        CompressorCreate(inst->sample_rate);
    CompressorSet((inst->compressor_bands + 3)->compressor, -15.0f, 2.0f, 1.0f,
                  50.0f, 0.0f);
    return;
end:
    CompressorBandFree(inst);
}

static void CreateMagneticEffect(MulCompressor* inst) {
    inst->nb_compressor_bands = 4;
    inst->compressor_bands = (CompressorBand*)calloc(inst->nb_compressor_bands,
                                                     sizeof(CompressorBand));
    if (NULL == inst->compressor_bands) return;

    // 设置第一个压缩器
    inst->compressor_bands->iir_band = (Band*)calloc(1, sizeof(Band));
    if (NULL == inst->compressor_bands->iir_band) goto end;
    iir_2nd_coeffs_butterworth_lowpass(inst->compressor_bands->iir_band,
                                       inst->sample_rate, 200.0f);
    inst->compressor_bands->compressor = CompressorCreate(inst->sample_rate);
    CompressorSet(inst->compressor_bands->compressor, -15.0f, 2.0f, 1.0f, 50.0f,
                  0.0f);

    // 设置第二个压缩器
    (inst->compressor_bands + 1)->iir_band = (Band*)calloc(1, sizeof(Band));
    if (NULL == (inst->compressor_bands + 1)->iir_band) goto end;
    iir_2nd_coeffs_butterworth_bandpass(
        (inst->compressor_bands + 1)->iir_band, inst->sample_rate,
        sqrt(200.0f * 1050.0f), sqrt(200.0f * 1050.0) / (1020.f - 200.0f));
    (inst->compressor_bands + 1)->compressor =
        CompressorCreate(inst->sample_rate);
    CompressorSet((inst->compressor_bands + 1)->compressor, -15.3f, 2.0f, 1.0f,
                  50.0f, 2.0f);

    // 设置第三个压缩器
    (inst->compressor_bands + 2)->iir_band = (Band*)calloc(1, sizeof(Band));
    if (NULL == (inst->compressor_bands + 2)->iir_band) goto end;
    iir_2nd_coeffs_butterworth_bandpass(
        (inst->compressor_bands + 2)->iir_band, inst->sample_rate,
        sqrt(1050.0f * 9150.0f), sqrt(1050.0f * 9150.0f) / (9150.0f - 1050.0f));
    (inst->compressor_bands + 2)->compressor =
        CompressorCreate(inst->sample_rate);
    CompressorSet((inst->compressor_bands + 2)->compressor, -20.0f, 2.0f, 1.0f,
                  50.0f, 3.0f);

    // 设置第四个压缩器
    (inst->compressor_bands + 3)->iir_band = (Band*)calloc(1, sizeof(Band));
    if (NULL == (inst->compressor_bands + 3)->iir_band) goto end;
    iir_2nd_coeffs_butterworth_highpass(inst->compressor_bands->iir_band,
                                        inst->sample_rate, 9150.0f);
    (inst->compressor_bands + 3)->compressor =
        CompressorCreate(inst->sample_rate);
    CompressorSet((inst->compressor_bands + 3)->compressor, -20.0f, 2.0f, 1.0f,
                  50.0f, 0.0f);
    return;
end:
    CompressorBandFree(inst);
}

static void CreateSoftPitch(MulCompressor* inst) {
    inst->nb_compressor_bands = 4;
    inst->compressor_bands = (CompressorBand*)calloc(inst->nb_compressor_bands,
                                                     sizeof(CompressorBand));
    if (NULL == inst->compressor_bands) return;

    // 设置第一个压缩器
    inst->compressor_bands->iir_band = (Band*)calloc(1, sizeof(Band));
    if (NULL == inst->compressor_bands->iir_band) goto end;
    iir_2nd_coeffs_butterworth_lowpass(inst->compressor_bands->iir_band,
                                       inst->sample_rate, 150.0f);
    inst->compressor_bands->compressor = CompressorCreate(inst->sample_rate);
    CompressorSet(inst->compressor_bands->compressor, -15.0f, 2.0f, 1.0f, 50.0f,
                  -5.0f);

    // 设置第二个压缩器
    (inst->compressor_bands + 1)->iir_band = (Band*)calloc(1, sizeof(Band));
    if (NULL == (inst->compressor_bands + 1)->iir_band) goto end;
    iir_2nd_coeffs_butterworth_bandpass(
        (inst->compressor_bands + 1)->iir_band, inst->sample_rate,
        sqrt(150.0f * 2820.0f), sqrt(150.0f * 2820.0) / (2820.f - 150.0f));
    (inst->compressor_bands + 1)->compressor =
        CompressorCreate(inst->sample_rate);
    CompressorSet((inst->compressor_bands + 1)->compressor, -15.0f, 2.0f, 1.0f,
                  50.0f, -3.0f);

    // 设置第三个压缩器
    (inst->compressor_bands + 2)->iir_band = (Band*)calloc(1, sizeof(Band));
    if (NULL == (inst->compressor_bands + 2)->iir_band) goto end;
    iir_2nd_coeffs_butterworth_bandpass(
        (inst->compressor_bands + 2)->iir_band, inst->sample_rate,
        sqrt(2820.0f * 9970.0f), sqrt(2820.0f * 9970.0f) / (9970.0f - 2820.0f));
    (inst->compressor_bands + 2)->compressor =
        CompressorCreate(inst->sample_rate);
    CompressorSet((inst->compressor_bands + 2)->compressor, -15.0f, 2.0f, 1.0f,
                  50.0f, 5.0f);

    // 设置第四个压缩器
    (inst->compressor_bands + 3)->iir_band = (Band*)calloc(1, sizeof(Band));
    if (NULL == (inst->compressor_bands + 3)->iir_band) goto end;
    iir_2nd_coeffs_butterworth_highpass(inst->compressor_bands->iir_band,
                                        inst->sample_rate, 9970.0f);
    (inst->compressor_bands + 3)->compressor =
        CompressorCreate(inst->sample_rate);
    CompressorSet((inst->compressor_bands + 3)->compressor, -15.0f, 2.0f, 1.0f,
                  50.0f, -1.0f);
    return;
end:
    CompressorBandFree(inst);
}

void MulCompressorSetMode(MulCompressor* inst,
                          const enum MulCompressMode mode) {
    CompressorBandFree(inst);
    switch (mode) {
        case MulComCleanVoice:
            // 清晰人声
            CreateCleanVoice(inst);
            break;
        case MulComBass:
            // 低音 -> 沉稳
            CreateBassEffect(inst);
            break;
        case MulComLowVoice:
            // 低沉 -> 低音
            CreateLowVoice(inst);
            break;
        case MulComPenetrating:
            // 穿透
            CreatePenetratingEffect(inst);
            break;
        case MulComMagnetic:
            // 磁性
            CreateMagneticEffect(inst);
            break;
        case MulComSoftPitch:
            // 柔和高音
            CreateSoftPitch(inst);
            break;
        default:
            break;
    }
}

static int CompressorBandProcess(CompressorBand* compressor_band, float* buffer,
                                 const int buffer_size) {
    // 拷贝数据
    memcpy(compressor_band->buffer, buffer, buffer_size * sizeof(float));
    // 分频
    band_process(compressor_band->iir_band, compressor_band->buffer,
                 buffer_size);
    // 压缩处理
    return CompressorProcess(compressor_band->compressor,
                             compressor_band->buffer, buffer_size);
}

static int RellocateBuffer(MulCompressor* inst, const int buffer_size) {
    for (int i = 0; i < inst->nb_compressor_bands; ++i) {
        if ((inst->compressor_bands + i)->buffer) {
            free((inst->compressor_bands + i)->buffer);
            (inst->compressor_bands + i)->buffer = NULL;
        }
        (inst->compressor_bands + i)->buffer =
            (float*)calloc(buffer_size, sizeof(float));
        if (NULL == (inst->compressor_bands + i)->buffer) {
            inst->max_nb_samples = 0;
            return -1;
        }
    }
    inst->max_nb_samples = buffer_size;
    return 0;
}

int MulCompressorProcess(MulCompressor* inst, float* buffer,
                         const int buffer_size) {
    if (NULL == inst || inst->nb_compressor_bands <= 0) return buffer_size;
    int ret = 0;

    // 为各个频段分配buffer
    if (buffer_size > inst->max_nb_samples) {
        if (RellocateBuffer(inst, buffer_size) < 0) return buffer_size;
    }

    // 分频处理
    for (int i = 0; i < inst->nb_compressor_bands; ++i) {
        ret = CompressorBandProcess(inst->compressor_bands + i, buffer,
                                    buffer_size);
    }

    // TODO: 合并
    memset(buffer, 0, sizeof(float) * buffer_size);
    for (int i = 0; i < ret; ++i) {
        for (int j = 0; j < inst->nb_compressor_bands; ++j) {
            buffer[i] += (inst->compressor_bands + j)->buffer[i];
        }
    }

    return ret;
}