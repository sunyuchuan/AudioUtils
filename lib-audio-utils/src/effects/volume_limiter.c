//
// Created by sunyc on 19-10-15.
//
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "effect_struct.h"
#include "error_def.h"
#include "log.h"
#include "beautify/limiter.h"
#include "tools/fifo.h"
#include "tools/sdl_mutex.h"
#include "tools/conversion.h"

typedef struct {
    Limiter *limiter;
    fifo *fifo_in;
    fifo *fifo_out;
    SdlMutex *sdl_mutex;

    float limiter_threshold_in_dB;
    float output_gain_in_dB;
    float attack_time_in_ms;
    float decay_time_in_ms;

    float flp_buffer[MAX_NB_SAMPLES];
    short fix_buffer[MAX_NB_SAMPLES];
    bool effect_on;
} priv_t;

static void init_parameter(priv_t *priv) {
    assert(NULL != priv);
    priv->limiter_threshold_in_dB = -0.5f;
    priv->output_gain_in_dB = 0.0f;
    priv->attack_time_in_ms = 0.0f;
    priv->decay_time_in_ms = 0.0f;
}

static int limiter_close(EffectContext *ctx) {
    LogInfo("%s.\n", __func__);
    assert(NULL != ctx);

    if (ctx->priv) {
        priv_t *priv = (priv_t *)ctx->priv;
        if (priv->limiter) LimiterFree(&priv->limiter);
        if (priv->fifo_in) fifo_delete(&priv->fifo_in);
        if (priv->fifo_out) fifo_delete(&priv->fifo_out);
        if (priv->sdl_mutex) sdl_mutex_free(&priv->sdl_mutex);
    }
    return 0;
}

static int limiter_init(EffectContext *ctx, int argc, const char **argv) {
    LogInfo("%s.\n", __func__);
    for (int i = 0; i < argc; ++i) {
        LogInfo("argv[%d] = %s\n", i, argv[i]);
    }
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    if (NULL == priv) return AEERROR_NULL_POINT;

    int ret = 0;
    priv->limiter = LimiterCreate(ctx->in_signal.sample_rate);
    if (NULL == priv->limiter) {
        ret = AEERROR_NOMEM;
        goto end;
    }
    LimiterSetSwitch(priv->limiter, 1);

    priv->fifo_in = fifo_create(sizeof(int16_t));
    if (NULL == priv->fifo_in) {
        ret = AEERROR_NOMEM;
        goto end;
    }
    priv->fifo_out = fifo_create(sizeof(int16_t));
    if (NULL == priv->fifo_out) {
        ret = AEERROR_NOMEM;
        goto end;
    }
    priv->sdl_mutex = sdl_mutex_create();
    if (NULL == priv->sdl_mutex) {
        ret = AEERROR_NOMEM;
        goto end;
    }

    init_parameter(priv);

end:
    if (ret < 0) limiter_close(ctx);
    return ret;
}

static int limiter_set(EffectContext *ctx, const char *key,
                                 int flags) {
    assert(NULL != ctx);

    priv_t *priv = ctx->priv;
    AEDictionaryEntry *entry = ae_dict_get(ctx->options, key, NULL, flags);
    if (entry) {
        LogInfo("%s key = %s val = %s\n", __func__, entry->key, entry->value);

        sdl_mutex_lock(priv->sdl_mutex);
        if (0 == strcasecmp(entry->key, "limiter_threshold_in_dB")) {
            priv->limiter_threshold_in_dB = strtod(entry->value, NULL);
        } else if (0 == strcasecmp(entry->key, "output_gain_in_dB")) {
            priv->output_gain_in_dB = strtod(entry->value, NULL);
        } else if (0 == strcasecmp(entry->key, "attack_time_in_ms")) {
            priv->attack_time_in_ms = strtod(entry->value, NULL);
        } else if (0 == strcasecmp(entry->key, "decay_time_in_ms")) {
            priv->decay_time_in_ms = strtod(entry->value, NULL);
        } else if (0 == strcasecmp(entry->key, "Switch")) {
            if (0 == strcasecmp(entry->value, "Off")) {
                priv->effect_on = false;
            } else if (0 == strcasecmp(entry->value, "On")) {
                priv->effect_on = true;
            }
        }
        sdl_mutex_unlock(priv->sdl_mutex);
    }

    LimiterSet(priv->limiter, priv->limiter_threshold_in_dB,
            priv->attack_time_in_ms, priv->decay_time_in_ms, priv->output_gain_in_dB);

    return 0;
}

static int limiter_send(EffectContext *ctx, const void *samples,
                                  const size_t nb_samples) {
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    assert(NULL != priv);
    assert(NULL != priv->fifo_in);

    if (priv->fifo_in) return fifo_write(priv->fifo_in, samples, nb_samples);
    return 0;
}

static int limiter_receive(EffectContext *ctx, void *samples,
                                     const size_t max_nb_samples) {
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    assert(NULL != priv);
    assert(NULL != priv->fifo_in);
    assert(NULL != priv->fifo_out);

    sdl_mutex_lock(priv->sdl_mutex);
    if (priv->effect_on) {
        int nb_samples = fifo_read(priv->fifo_in, priv->fix_buffer, MAX_NB_SAMPLES);
        while (nb_samples > 0) {
            S16ToFloat(priv->fix_buffer, priv->flp_buffer, nb_samples);
            nb_samples = LimiterProcess(priv->limiter, priv->flp_buffer, nb_samples);
            FloatToS16(priv->flp_buffer, priv->fix_buffer, nb_samples);
            fifo_write(priv->fifo_out, priv->fix_buffer, nb_samples);
            nb_samples = fifo_read(priv->fifo_in, priv->fix_buffer, MAX_NB_SAMPLES);
        }
    } else {
        while (fifo_occupancy(priv->fifo_in) > 0) {
            int nb_samples = fifo_read(priv->fifo_in, priv->fix_buffer, MAX_NB_SAMPLES);
            fifo_write(priv->fifo_out, priv->fix_buffer, nb_samples);
        }
    }
    sdl_mutex_unlock(priv->sdl_mutex);

    if (atomic_load(&ctx->return_max_nb_samples) &&
        fifo_occupancy(priv->fifo_out) < max_nb_samples)
        return 0;
    return fifo_read(priv->fifo_out, samples, max_nb_samples);
}

const EffectHandler *effect_limiter_fn(void) {
    static EffectHandler handler = {.name = "limiter",
                                    .usage = "",
                                    .priv_size = sizeof(priv_t),
                                    .init = limiter_init,
                                    .set = limiter_set,
                                    .send = limiter_send,
                                    .receive = limiter_receive,
                                    .close = limiter_close};
    return &handler;
}

