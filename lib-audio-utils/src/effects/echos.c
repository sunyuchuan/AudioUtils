/*
 * Flow diagram scheme for n delays ( 1 <= n <= MAX_ECHOS ):
 *
 *                                                    * gain-in  ___
 * ibuff --+--------------------------------------------------->|   |
 *         |                                          * decay 1 |   |
 *         |               +----------------------------------->|   |
 *         |               |                          * decay 2 | + |
 *         |               |             +--------------------->|   |
 *         |               |             |            * decay n |   |
 *         |    _________  |  _________  |     _________   +--->|___|
 *         |   |         | | |         | |    |         |  |      |
 *         +-->| delay 1 |-+-| delay 2 |-+...-| delay n |--+      | * gain-out
 *             |_________|   |_________|      |_________|         |
 *                                                                +----->obuff
 * Usage:
 *   echos gain-in gain-out delay-1 decay-1 [delay-2 decay-2 ... delay-n
 * decay-n]
 *
 * Where:
 *   gain-in, decay-1 ... decay-n :  0.0 ... 1.0      volume
 *   gain-out :  0.0 ...      volume
 *   delay-1 ... delay-n :  > 0.0 msec
 *
 * Note:
 *   when decay is close to 1.0, the samples can begin clipping and the output
 *   can saturate!
 *
 * Hint:
 *   1 / out-gain > gain-in ( 1 + decay-1 + ... + decay-n )
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "effect_struct.h"
#include "tools/fifo.h"
#include "tools/sdl_mutex.h"

#define DELAY_BUFSIZ (50 * 50U * 1024)
#define MAX_ECHOS 7 /* 24 bit x ( 1 + MAX_ECHOS ) = */
                    /* 24 bit x 8 = 32 bit !!!      */

typedef struct {
    fifo *fifo_in;
    fifo *fifo_out;
    SdlMutex *sdl_mutex;
    bool effect_on;

    int counter[MAX_ECHOS];
    int num_delays;
    sample_type *delay_buf;
    float in_gain, out_gain;
    float delay[MAX_ECHOS], decay[MAX_ECHOS];
    ptrdiff_t samples[MAX_ECHOS], pointer[MAX_ECHOS];
    size_t sumsamples;
} priv_t;

static int echos_getopts(EffectContext *ctx, int argc, const char **argv) {
    priv_t *priv = (priv_t *)ctx->priv;
    int i = 0;
    priv->num_delays = 0;

    --argc, ++argv;
    if ((argc < 2) || (argc % 2)) {
        LogError("%s\n", show_usage(ctx));
        return AUDIO_EFFECT_EOF;
    }

    i = 0;
    sscanf(argv[i++], "%f", &priv->in_gain);
    sscanf(argv[i++], "%f", &priv->out_gain);
    while (i < argc) {
        /* Linux bug and it's cleaner. */
        sscanf(argv[i++], "%f", &priv->delay[priv->num_delays]);
        sscanf(argv[i++], "%f", &priv->decay[priv->num_delays]);
        priv->num_delays++;
        if (priv->num_delays > MAX_ECHOS) {
            LogError("echos: to many delays, use less than %i delays",
                     MAX_ECHOS);
            return AUDIO_EFFECT_EOF;
        }
    }
    priv->sumsamples = 0;
    return AUDIO_EFFECT_SUCCESS;
}

static int echos_start(EffectContext *ctx) {
    priv_t *priv = (priv_t *)ctx->priv;
    float sum_in_volume;

    if (priv->in_gain < 0.0) {
        LogError("echos: gain-in must be positive!");
        return AUDIO_EFFECT_EOF;
    }
    if (priv->in_gain > 1.0) {
        LogError("echos: gain-in must be less than 1.0!");
        return AUDIO_EFFECT_EOF;
    }
    if (priv->out_gain < 0.0) {
        LogError("echos: gain-in must be positive!");
        return AUDIO_EFFECT_EOF;
    }
    for (int i = 0; i < priv->num_delays; i++) {
        priv->samples[i] = priv->delay[i] * ctx->in_signal.sample_rate / 1000.0;
        if (priv->samples[i] < 1) {
            LogError("echos: delay must be positive!");
            return AUDIO_EFFECT_EOF;
        }
        if (priv->samples[i] > (ptrdiff_t)DELAY_BUFSIZ) {
            LogError("echos: delay must be less than %g seconds!",
                     DELAY_BUFSIZ / ctx->in_signal.sample_rate);
            return AUDIO_EFFECT_EOF;
        }
        if (priv->decay[i] < 0.0) {
            LogError("echos: decay must be positive!");
            return AUDIO_EFFECT_EOF;
        }
        if (priv->decay[i] > 1.0) {
            LogError("echos: decay must be less than 1.0!");
            return AUDIO_EFFECT_EOF;
        }
        priv->counter[i] = 0;
        priv->pointer[i] = priv->sumsamples;
        priv->sumsamples += priv->samples[i];
    }
    if (priv->delay_buf) {
        free(priv->delay_buf);
        priv->delay_buf = NULL;
    }
    priv->delay_buf = calloc(priv->sumsamples, sizeof(sample_type));
    /* Be nice and check the hint with warning, if... */
    sum_in_volume = 1.0;
    for (int i = 0; i < priv->num_delays; i++) sum_in_volume += priv->decay[i];
    if (sum_in_volume * priv->in_gain > 1.0 / priv->out_gain)
        LogWarning(
            "echos: warning >>> gain-out can cause saturation of output <<<");
    priv->effect_on = priv->num_delays > 0;

    return AUDIO_EFFECT_SUCCESS;
}

static int echos_parseopts(EffectContext *ctx, const char *argvs) {
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
    int ret = echos_getopts(ctx, argc, argv);
    if (ret < 0) goto end;
    ret = echos_start(ctx);

end:
    if (argvs2) free(argvs2);
    return ret;
}

static int echos_close(EffectContext *ctx) {
    LogInfo("%s.\n", __func__);
    assert(NULL != ctx);

    if (ctx->priv) {
        priv_t *priv = (priv_t *)ctx->priv;
        if (priv->fifo_in) fifo_delete(&priv->fifo_in);
        if (priv->fifo_out) fifo_delete(&priv->fifo_out);
        if (priv->sdl_mutex) sdl_mutex_free(&priv->sdl_mutex);
        if (priv->delay_buf) {
            free(priv->delay_buf);
            priv->delay_buf = NULL;
        }
    }
    return 0;
}

static int echos_init(EffectContext *ctx, int argc, const char **argv) {
    LogInfo("%s.\n", __func__);
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    if (NULL == priv) return AEERROR_NULL_POINT;

    int ret = 0;
    priv->fifo_in = fifo_create(sizeof(sample_type));
    if (NULL == priv->fifo_in) {
        ret = AEERROR_NOMEM;
        goto end;
    }
    priv->fifo_out = fifo_create(sizeof(sample_type));
    if (NULL == priv->fifo_out) {
        ret = AEERROR_NOMEM;
        goto end;
    }
    priv->sdl_mutex = sdl_mutex_create();
    if (NULL == priv->sdl_mutex) {
        ret = AEERROR_NOMEM;
        goto end;
    }

    if (argc > 1 && argv != NULL) {
        ret = echos_getopts(ctx, argc, argv);
        if (ret < 0) goto end;
    } else {
        priv->in_gain = priv->out_gain = 1.0f;
    }

    return echos_start(ctx);

end:
    if (ret < 0) echos_close(ctx);
    return ret;
}

static int echos_set(EffectContext *ctx, const char *key, int flags) {
    assert(NULL != ctx);

    int ret = 0;
    priv_t *priv = ctx->priv;
    AEDictionaryEntry *entry = ae_dict_get(ctx->options, key, NULL, flags);
    if (entry) {
        LogInfo("%s key = %s val = %s\n", __func__, entry->key, entry->value);

        sdl_mutex_lock(priv->sdl_mutex);
        if (0 == strcasecmp(entry->key, ctx->handler.name)) {
            ret = echos_parseopts(ctx, entry->value);
        } else if (0 == strcasecmp(entry->key, "Switch")) {
            if (0 == strcasecmp(entry->value, "Off")) {
                priv->effect_on = false;
            } else if (0 == strcasecmp(entry->value, "On")) {
                priv->effect_on = true;
            }
        }
        sdl_mutex_unlock(priv->sdl_mutex);
    }
    return ret;
}

static int echos_send(EffectContext *ctx, const void *samples,
                      const size_t nb_samples) {
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    assert(NULL != priv);
    assert(NULL != priv->fifo_in);

    return fifo_write(priv->fifo_in, samples, nb_samples);
}

static int echos_receive(EffectContext *ctx, void *samples,
                         const size_t max_nb_samples) {
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    assert(NULL != priv);
    assert(NULL != priv->fifo_in);

    // LogError("%d\n", priv->effect_on);
    sdl_mutex_lock(priv->sdl_mutex);
    if (priv->effect_on) {
        size_t nb_samples = fifo_read(priv->fifo_in, samples, max_nb_samples);
        sample_type *ibuf = samples;
        sample_type out;
        for (size_t i = 0; i < nb_samples; ++i) {
            out = *ibuf * priv->in_gain;
            for (int j = 0; j < priv->num_delays; ++j) {
                out += priv->delay_buf[priv->counter[j] + priv->pointer[j]] *
                       priv->decay[j];
            }
            out *= priv->out_gain;
            /* Mix decay of delays and input */
            for (int j = 0; j < priv->num_delays; j++) {
                if (j == 0)
                    priv->delay_buf[priv->counter[j] + priv->pointer[j]] =
                        *ibuf;
                else
                    priv->delay_buf[priv->counter[j] + priv->pointer[j]] =
                        priv->delay_buf[priv->counter[j - 1] +
                                        priv->pointer[j - 1]] +
                        *ibuf;
            }
            /* Adjust the counters */
            for (int j = 0; j < priv->num_delays; j++)
                priv->counter[j] = (priv->counter[j] + 1) % priv->samples[j];
            *ibuf++ = out;
        }
        fifo_write(priv->fifo_out, samples, nb_samples);
    } else {
        while (fifo_occupancy(priv->fifo_in) > 0) {
            size_t nb_samples =
                fifo_read(priv->fifo_in, samples, max_nb_samples);
            fifo_write(priv->fifo_out, samples, nb_samples);
        }
    }
    sdl_mutex_unlock(priv->sdl_mutex);

    if (atomic_load(&ctx->return_max_nb_samples) &&
        fifo_occupancy(priv->fifo_out) < max_nb_samples)
        return 0;
    return fifo_read(priv->fifo_out, samples, max_nb_samples);
}

const EffectHandler *effect_echos_fn(void) {
    static EffectHandler handler = {
        .name = "echos",
        .usage = "gain-in gain-out delay decay [ delay decay ... ]",
        .priv_size = sizeof(priv_t),
        .init = echos_init,
        .set = echos_set,
        .send = echos_send,
        .receive = echos_receive,
        .close = echos_close};
    return &handler;
}
