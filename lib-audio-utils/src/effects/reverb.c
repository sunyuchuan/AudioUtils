#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "effect_struct.h"
#include "tools/fifo.h"
#include "tools/sdl_mutex.h"
#include "tools/util.h"
#include "tools/conversion.h"

#define MAX_SAMPLE_SIZE 2048

typedef struct {
    size_t size;
    sample_type *buffer;
    sample_type *ptr;
    sample_type store;
} filter_t;

/* gcc -O2 will inline this */
static sample_type comb_process(filter_t *filter, const sample_type input,
                                const float feedback, const float hf_damping) {
    sample_type output = *filter->ptr;
    filter->store = output + (filter->store - output) * hf_damping;
    *filter->ptr = input + filter->store * feedback;
    if (--filter->ptr < filter->buffer) filter->ptr += filter->size;
    return output;
}

/* gcc -O2 will inline this */
static sample_type allpass_process(filter_t *filter, const sample_type input) {
    sample_type output = *filter->ptr;
    *filter->ptr = input + output * 0.5f;
    if (--filter->ptr < filter->buffer) filter->ptr += filter->size;
    return output - input;
}

/* Filter delay lengths in samples (44100Hz sample-rate) */
static const size_t comb_lengths[] = {1116, 1188, 1277, 1356,
                                      1422, 1491, 1557, 1617};
static const size_t allpass_lengths[] = {225, 341, 441, 556};

typedef struct {
    filter_t comb[array_length(comb_lengths)];
    filter_t allpass[array_length(allpass_lengths)];
} filter_array_t;

static void filter_array_create(filter_array_t *p, float sample_rate,
                                float room_scale) {
    /* Compensate for actual sample-rate */
    float r = sample_rate * (1 / 44100.0f);

    for (size_t i = 0; i < array_length(comb_lengths); ++i) {
        filter_t *pcomb = &p->comb[i];
        pcomb->size = (size_t)(room_scale * r * (comb_lengths[i]) + 0.5f);
        pcomb->ptr = pcomb->buffer =
            (sample_type *)calloc(pcomb->size, sizeof(sample_type));
    }
    for (size_t i = 0; i < array_length(allpass_lengths); ++i) {
        filter_t *pallpass = &p->allpass[i];
        pallpass->size = (size_t)(r * (allpass_lengths[i]) + 0.5f);
        pallpass->ptr = pallpass->buffer =
            (sample_type *)calloc(pallpass->size, sizeof(sample_type));
    }
}

static void filter_array_process(filter_array_t *p, size_t length,
                                 sample_type const *input, sample_type *output,
                                 float const feedback, float const hf_damping,
                                 float const gain) {
    while (length--) {
        sample_type out = 0.0f;
        sample_type in = *input++;
        size_t i = array_length(comb_lengths) - 1;
        do {
            out += comb_process(p->comb + i, in, feedback, hf_damping);
        } while (i--);

        i = array_length(allpass_lengths) - 1;
        do {
            out = allpass_process(p->allpass + i, out);
        } while (i--);
        *output++ = out * gain;
    }
}

static void filter_array_delete(filter_array_t *p) {
    for (size_t i = 0; i < array_length(allpass_lengths); ++i) {
        if ((&p->allpass[i])->buffer) {
            free((&p->allpass[i])->buffer);
            (&p->allpass[i])->buffer = NULL;
        }
    }
    for (size_t i = 0; i < array_length(comb_lengths); ++i) {
        if ((&p->comb[i])->buffer) {
            free((&p->comb[i])->buffer);
            (&p->comb[i])->buffer = NULL;
        }
    }
}

typedef struct {
    fifo *fifo_in;
    fifo *fifo_out;
    SdlMutex *sdl_mutex;
    bool effect_on;

    bool wet_only;
    float reverberance;
    float hf_damping;
    float room_scale;
    float pre_delay_ms;
    float wet_gain_dB;

    float feedback;
    float gain;
    size_t delay_samples;
    filter_array_t filter_array;

    short *fix_buffer;

    sample_type dry_buf[MAX_SAMPLE_SIZE];
    sample_type wet_buf[MAX_SAMPLE_SIZE];
} priv_t;

static int reverb_getopts(EffectContext *ctx, int argc, const char **argv) {
    LogInfo("%s.\n", __func__);
    priv_t *priv = (priv_t *)ctx->priv;

    priv->reverberance = priv->hf_damping = 50; /* Set non-zero defaults */
    priv->room_scale = 100;

    --argc, ++argv;
    priv->wet_only = argc &&
                     (!strcmp(*argv, "-w") || !strcmp(*argv, "--wet-only")) &&
                     (--argc, ++argv, true);
    do { /* break-able block */
        NUMERIC_PARAMETER(reverberance, 0, 100)
        NUMERIC_PARAMETER(hf_damping, 0, 100)
        NUMERIC_PARAMETER(room_scale, 0, 100)
        NUMERIC_PARAMETER(pre_delay_ms, 0, 500)
        NUMERIC_PARAMETER(wet_gain_dB, -10, 10)
    } while (0);

    return argc ? AUDIO_EFFECT_EOF : AUDIO_EFFECT_SUCCESS;
}

static int reverb_start(EffectContext *ctx) {
    LogInfo("%s.\n", __func__);
    priv_t *priv = (priv_t *)ctx->priv;
    size_t delay_samples =
        priv->pre_delay_ms / 1000 * ctx->in_signal.sample_rate + 0.5f;
    float scale = priv->room_scale / 100.0f * 0.9f + 0.1f;
    /* Set minimum feedback */
    float a = -1 / log(1 - /**/ 0.3f /**/);
    /* Set maximum feedback */
    float b = 100 / (log(1 - /**/ 0.98f /**/) * a + 1);

    priv->hf_damping = priv->hf_damping / 100 * 0.3f + 0.2f;
    priv->feedback = 1 - expf((priv->reverberance - b) / (a * b));
    priv->gain = dB_to_linear(priv->wet_gain_dB) * 0.015f;

    // 预留delay_samples
    if (priv->delay_samples < delay_samples) {
        short *tmp_samples = (short *)calloc(
            delay_samples - priv->delay_samples, sizeof(short));
        fifo_write(priv->fifo_in, tmp_samples,
                   delay_samples - priv->delay_samples);
        free(tmp_samples);
    } else if (priv->delay_samples > delay_samples) {
        short *tmp_samples = (short *)calloc(
            delay_samples - priv->delay_samples, sizeof(short));
        fifo_read(priv->fifo_in, tmp_samples,
                  delay_samples - priv->delay_samples);
        free(tmp_samples);
    }
    priv->delay_samples = delay_samples;

    filter_array_delete(&priv->filter_array);
    filter_array_create(&priv->filter_array, ctx->in_signal.sample_rate, scale);
    return AUDIO_EFFECT_SUCCESS;
}

static int reverb_parseopts(EffectContext *ctx, const char *argvs) {
#define MAX_ARGC 50
    const char *argv[MAX_ARGC];
    int argc = 0;
    argv[argc++] = ctx->handler.name;

    char *argvs2 = calloc(strlen(argvs) + 1, sizeof(char));
    memcpy(argvs2, argvs, strlen(argvs) + 1);
    char *token = strtok(argvs2, " ");

    while (token != NULL) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }
    int ret = reverb_getopts(ctx, argc, argv);
    if (ret < 0) goto end;
    ret = reverb_start(ctx);

end:
    if (argvs2) free(argvs2);
    return ret;
}

static int reverb_close(EffectContext *ctx) {
    LogInfo("%s.\n", __func__);
    assert(NULL != ctx);

    if (ctx->priv) {
        priv_t *priv = (priv_t *)ctx->priv;
        if (priv->fifo_in) fifo_delete(&priv->fifo_in);
        if (priv->fifo_out) fifo_delete(&priv->fifo_out);
        if (priv->sdl_mutex) sdl_mutex_free(&priv->sdl_mutex);
        if (priv->fix_buffer) {
            free(priv->fix_buffer);
            priv->fix_buffer = NULL;
        }
        filter_array_delete(&priv->filter_array);
    }
    return 0;
}

static int reverb_init(EffectContext *ctx, int argc, const char **argv) {
    LogInfo("%s.\n", __func__);
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    if (NULL == priv) return AEERROR_NULL_POINT;

    int ret = 0;
    priv->fifo_in = fifo_create(sizeof(short));
    if (NULL == priv->fifo_in) {
        ret = AEERROR_NOMEM;
        goto end;
    }
    priv->fifo_out = fifo_create(sizeof(short));
    if (NULL == priv->fifo_out) {
        ret = AEERROR_NOMEM;
        goto end;
    }
    priv->sdl_mutex = sdl_mutex_create();
    if (NULL == priv->sdl_mutex) {
        ret = AEERROR_NOMEM;
        goto end;
    }

    priv->fix_buffer = (short *)calloc(MAX_SAMPLE_SIZE, sizeof(short));
    if (NULL == priv->fix_buffer) {
        LogError("%s calloc fix_buffer failed.\n", __func__);
        goto end;
    }

    if (argc > 1 && argv != NULL) {
        ret = reverb_getopts(ctx, argc, argv);
        if (ret < 0) goto end;
    }

    return reverb_start(ctx);

end:
    if (ret < 0) reverb_close(ctx);
    return ret;
}

static void reverb_set_mode(EffectContext *ctx, const char *mode) {
    LogInfo("%s mode = %s.\n", __func__, mode);
    priv_t *priv = (priv_t *)ctx->priv;
    if (0 == strcasecmp(mode, "Original")) {
        priv->effect_on = false;
    } else {
        reverb_parseopts(ctx, REVERB_PARAMS);
        priv->effect_on = true;
    }
}

static int reverb_set(EffectContext *ctx, const char *key, int flags) {
    assert(NULL != ctx);

    int ret = 0;
    priv_t *priv = (priv_t *)ctx->priv;
    AEDictionaryEntry *entry = ae_dict_get(ctx->options, key, NULL, flags);
    if (entry) {
        LogInfo("%s key = %s val = %s\n", __func__, entry->key, entry->value);

        sdl_mutex_lock(priv->sdl_mutex);
        if (0 == strcasecmp(entry->key, ctx->handler.name)) {
            ret = reverb_parseopts(ctx, entry->value);
        } else if (0 == strcasecmp(entry->key, "Switch")) {
            if (0 == strcasecmp(entry->value, "Off")) {
                priv->effect_on = false;
            } else if (0 == strcasecmp(entry->value, "On")) {
                priv->effect_on = true;
            }
        } else if (0 == strcasecmp(entry->key, "mode")) {
            reverb_set_mode(ctx, entry->value);
        }

        sdl_mutex_unlock(priv->sdl_mutex);
    }
    return ret;
}

static int reverb_send(EffectContext *ctx, const void *samples,
                       const size_t nb_samples) {
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    assert(NULL != priv);
    assert(NULL != priv->fifo_in);

    return fifo_write(priv->fifo_in, samples, nb_samples);
}

static int reverb_receive(EffectContext *ctx, void *samples,
                          const size_t max_nb_samples) {
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    assert(NULL != priv);
    assert(NULL != priv->fifo_in);

    sdl_mutex_lock(priv->sdl_mutex);
    if (priv->effect_on) {
        size_t nb_samples =
            fifo_read(priv->fifo_in, priv->fix_buffer, MAX_SAMPLE_SIZE);
        S16ToFloat(priv->fix_buffer, priv->dry_buf, nb_samples);
        while (nb_samples > 0) {
            filter_array_process(&priv->filter_array, nb_samples, priv->dry_buf,
                                 priv->wet_buf, priv->feedback,
                                 priv->hf_damping, priv->gain);
            if (!priv->wet_only) {
                sample_type *dry_buffer = priv->dry_buf;
                sample_type *wet_buffer = priv->wet_buf;
                for (size_t i = 0; i < nb_samples; ++i) {
                    *wet_buffer = *wet_buffer + *dry_buffer;
                    ++dry_buffer;
                    ++wet_buffer;
                }
            }
            FloatToS16(priv->wet_buf, priv->fix_buffer, nb_samples);
            fifo_write(priv->fifo_out, priv->fix_buffer, nb_samples);
            nb_samples =
                fifo_read(priv->fifo_in, priv->fix_buffer, MAX_SAMPLE_SIZE);
            S16ToFloat(priv->fix_buffer, priv->dry_buf, nb_samples);
        }
    } else {
        while (fifo_occupancy(priv->fifo_in) > 0) {
            size_t nb_samples =
                fifo_read(priv->fifo_in, priv->fix_buffer, max_nb_samples);
            fifo_write(priv->fifo_out, priv->fix_buffer, nb_samples);
        }
    }
    sdl_mutex_unlock(priv->sdl_mutex);

    if (atomic_load(&ctx->return_max_nb_samples) &&
        fifo_occupancy(priv->fifo_out) < max_nb_samples)
        return 0;

    return fifo_read(priv->fifo_out, samples, max_nb_samples);
}

const EffectHandler *effect_reverb_fn(void) {
    static EffectHandler handler = {.name = "reverb",
                                    .usage =
                                        "[-w|--wet-only]"
                                        " [reverberance (50%)"
                                        " [HF-damping (50%)"
                                        " [room-scale (100%)"
                                        " [pre-delay (0ms)"
                                        " [wet-gain (0dB)"
                                        "]]]]]",
                                    .priv_size = sizeof(priv_t),
                                    .init = reverb_init,
                                    .set = reverb_set,
                                    .send = reverb_send,
                                    .receive = reverb_receive,
                                    .close = reverb_close};
    return &handler;
}
