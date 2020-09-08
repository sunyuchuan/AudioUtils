#include "flanger.h"
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923 /* pi/2 */
#endif

typedef enum { SOX_SHORT, SOX_INT, SOX_FLOAT, SOX_DOUBLE } sox_data_t;

struct FlangerT {
    /* Parameters */
    float delay_min;
    float delay_depth;
    float feedback_gain;
    float delay_gain;
    float speed;
    wave_t wave_shape;
    float channel_phase;
    interp_t interpolation;

    /* Delay buffers */
    float* delay_bufs;
    size_t delay_buf_length;
    size_t delay_buf_pos;
    float delay_last;

    /* Low Frequency Oscillator */
    float* lfo;
    size_t lfo_length;
    size_t lfo_pos;

    /* Balancing */
    float in_gain;

    int sample_rate;
};

// "[delay depth regen width speed shape phase interp]",
// "                  .",
// "                 /|regen",
// "                / |",
// "            +--(  |------------+",
// "            |   \\ |            |   .",
// "           _V_   \\|  _______   |   |\\ width   ___",
// "          |   |   ' |       |  |   | \\       |   |",
// "      +-->| + |---->| DELAY |--+-->|  )----->|   |",
// "      |   |___|     |_______|      | /       |   |",
// "      |           delay : depth    |/        |   |",
// "  In  |                 : interp   '         |   | Out",
// "  --->+               __:__                  | + |--->",
// "      |              |     |speed            |   |",
// "      |              |  ~  |shape            |   |",
// "      |              |_____|phase            |   |",
// "      +------------------------------------->|   |",
// "                                             |___|",
// "       RANGE DEFAULT DESCRIPTION",
// "delay   0 30    0    base delay in milliseconds",
// "depth   0 10    2    added swept delay in milliseconds",
// "regen -95 +95   0    percentage regeneration (delayed signal "
// "feedback)",
// "width   0 100   71   percentage of delayed signal mixed with original",
// "speed  0.1 10  0.5   sweeps per second (Hz) ",
// "shape    --    sin   swept wave shape: sine|triangle",
// "phase   0 100   25   swept wave percentage phase-shift for "
// "multi-channel",
// "                     (e.g. stereo) flange; 0 = 100 = same phase on "
// "each channel",
// "interp   --    lin   delay-line interpolation: linear|quadratic"

Flanger* FlangerCreate(const int sample_rate) {
    Flanger* self = (Flanger*)calloc(1, sizeof(Flanger));
    if (NULL == self) return NULL;

    self->sample_rate = sample_rate;
    FlangerSet(self, 0.0f, 0.0f, 0.0f, 0.0f, 0.1f, WAVE_SINE, 0.0f);
    return self;
}

void FlangerFree(Flanger** inst) {
    if (NULL == inst || NULL == *inst) return;

    Flanger* self = *inst;
    if (self->delay_bufs) {
        free(self->delay_bufs);
        self->delay_bufs = NULL;
    }
    if (self->lfo) {
        free(self->lfo);
        self->lfo = NULL;
    }
    free(*inst);
    *inst = NULL;
}

static void lsx_generate_wave_table(wave_t wave_type, sox_data_t data_type,
                                    void* table, size_t table_size, float min,
                                    float max, float phase) {
    uint32_t t;
    uint32_t phase_offset = phase / M_PI / 2 * table_size + 0.5;

    for (t = 0; t < table_size; t++) {
        uint32_t point = (t + phase_offset) % table_size;
        float d;
        switch (wave_type) {
            case WAVE_SINE:
                d = (sin((float)point / table_size * 2 * M_PI) + 1) / 2;
                break;

            case WAVE_TRIANGLE:
                d = (float)point * 2 / table_size;
                switch (4 * point / table_size) {
                    case 0:
                        d = d + 0.5;
                        break;
                    case 1:
                    case 2:
                        d = 1.5 - d;
                        break;
                    case 3:
                        d = d - 1.5;
                        break;
                }
                break;

            default:     /* Oops! FIXME */
                d = 0.0; /* Make sure we have a value */
                break;
        }
        d = d * (max - min) + min;
        switch (data_type) {
            case SOX_FLOAT: {
                float* fp = (float*)table;
                *fp++ = (float)d;
                table = fp;
                continue;
            }
            case SOX_DOUBLE: {
                double* dp = (double*)table;
                *dp++ = d;
                table = dp;
                continue;
            }
            default:
                break;
        }
        d += d < 0 ? -0.5 : +0.5;
        switch (data_type) {
            case SOX_SHORT: {
                short* sp = table;
                *sp++ = (short)d;
                table = sp;
                continue;
            }
            case SOX_INT: {
                int* ip = table;
                *ip++ = (int)d;
                table = ip;
                continue;
            }
            default:
                break;
        }
    }
}

void FlangerSet(Flanger* inst, const float delay, const float depth,
                const float regen, const float width, const float speed,
                const wave_t shape, const float phase) {
    inst->delay_min = delay;
    inst->delay_depth = depth;
    inst->feedback_gain = regen;
    inst->delay_gain = width;
    inst->speed = speed;
    inst->wave_shape = shape;
    inst->channel_phase = phase;
    inst->interpolation = INTERP_LINEAR;

    /* Scale to unity: */
    inst->feedback_gain /= 100;
    inst->delay_gain /= 100;
    inst->channel_phase /= 100;
    inst->delay_min /= 1000;
    inst->delay_depth /= 1000;

    /* Balance output: */
    inst->in_gain = 1 / (1 + inst->delay_gain);
    inst->delay_gain /= 1 + inst->delay_gain;

    /* Balance feedback loop: */
    inst->delay_gain *= 1 - fabs(inst->feedback_gain);
    /* Create the delay buffers, one for each channel: */
    inst->delay_buf_length =
        (inst->delay_min + inst->delay_depth) * inst->sample_rate + 0.5;
    ++inst->delay_buf_length; /* Need 0 to n, i.e. n + 1. */
    ++inst->delay_buf_length; /* Quadratic interpolator needs one more. */
    if (inst->delay_bufs) {
        free(inst->delay_bufs);
        inst->delay_bufs = NULL;
    }
    inst->delay_bufs = (float*)calloc(inst->delay_buf_length, sizeof(float));

    /* Create the LFO lookup table: */
    inst->lfo_length = inst->sample_rate / inst->speed;
    if (inst->lfo) {
        free(inst->lfo);
        inst->lfo = NULL;
    }
    inst->lfo = (float*)calloc(inst->lfo_length, sizeof(float));
    lsx_generate_wave_table(
        inst->wave_shape, SOX_FLOAT, inst->lfo, inst->lfo_length,
        floor(inst->delay_min * inst->sample_rate + .5),
        inst->delay_buf_length - 2.,
        3 * M_PI_2); /* Start the sweep at minimum delay (for mono at least) */
}

void FlangerProcess(Flanger* inst, float* buffer, const int buffer_size) {
    if (NULL == inst || NULL == buffer || buffer_size <= 0) return;

    for (int i = 0; i < buffer_size; ++i) {
        size_t channel_phase = inst->lfo_length * inst->channel_phase + .5;
        double delay =
            inst->lfo[(inst->lfo_pos + channel_phase) % inst->lfo_length];
        float frac_delay = modf(delay, &delay);
        size_t int_delay = (size_t)delay;

        inst->delay_buf_pos =
            (inst->delay_buf_pos + inst->delay_buf_length - 1) %
            inst->delay_buf_length;
        inst->delay_bufs[inst->delay_buf_pos] =
            buffer[i] + inst->delay_last * inst->feedback_gain;

        float delayed_0 = inst->delay_bufs[(inst->delay_buf_pos + int_delay++) %
                                           inst->delay_buf_length];
        float delayed_1 = inst->delay_bufs[(inst->delay_buf_pos + int_delay++) %
                                           inst->delay_buf_length];
        float delayed;

        if (INTERP_LINEAR == inst->interpolation) {
            delayed = delayed_0 + (delayed_1 - delayed_0) * frac_delay;
        } else {
            float a, b;
            float delayed_2 =
                inst->delay_bufs[(inst->delay_buf_pos + int_delay++) %
                                 inst->delay_buf_length];
            delayed_2 -= delayed_0;
            delayed_1 -= delayed_0;
            a = delayed_2 * .5 - delayed_1;
            b = delayed_1 * 2 - delayed_2 * .5;
            delayed = delayed_0 + (a * frac_delay + b) * frac_delay;
        }
        inst->delay_last = delayed;
        buffer[i] = buffer[i] * inst->in_gain + delayed * inst->delay_gain;
        inst->lfo_pos = (inst->lfo_pos + 1) % inst->lfo_length;
    }
}