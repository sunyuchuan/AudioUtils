#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "effect_struct.h"
#include "error_def.h"
#include "log.h"
#include "noise_suppression/noise_suppression.h"
#include "tools/fifo.h"
#include "tools/sdl_mutex.h"
#include "tools/util.h"

#define NB_SAMPLES 1024

typedef struct {
    NsHandle *ns;
    fifo *fifo_in;
    fifo *fifo_out;
    int16_t *in_buf;
    int16_t *out_buf;
    SdlMutex *sdl_mutex;
    bool is_noise_suppression_on;
} priv_t;

static int noise_suppression_close(EffectContext *ctx) {
    LogInfo("%s.\n", __func__);
    if(NULL == ctx) return -1;

    if (ctx->priv) {
        priv_t *priv = (priv_t *)ctx->priv;
        if (priv->ns) {
            XmNs_Free(priv->ns);
            priv->ns = NULL;
        }
        if (priv->fifo_in) fifo_delete(&priv->fifo_in);
        if (priv->fifo_out) fifo_delete(&priv->fifo_out);
        if (priv->in_buf) {
            free(priv->in_buf);
            priv->in_buf = NULL;
        }
        if (priv->out_buf) {
            free(priv->out_buf);
            priv->out_buf = NULL;
        }
        if (priv->sdl_mutex) sdl_mutex_free(&priv->sdl_mutex);
    }

    return 0;
}

static int noise_suppression_init(EffectContext *ctx,
        int argc, const char **argv) {
    LogInfo("%s.\n", __func__);
    for (int i = 0; i < argc; ++i) {
        LogInfo("argv[%d] = %s\n", i, argv[i]);
    }
    if(NULL == ctx) {return -1;}
    priv_t *priv = (priv_t *)ctx->priv;
    if (NULL == priv) return AEERROR_NULL_POINT;

    int ret = 0;

    if ((ret = XmNs_Create(&priv->ns)) < 0) {
        goto end;
    }
    if ((ret = XmNs_Init(priv->ns, ctx->in_signal.sample_rate)) < 0) {
        goto end;
    }
    if ((ret = XmNs_set_policy(priv->ns, NS_MODE_LEVEL_3)) < 0) {
        goto end;
    }

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
    priv->in_buf = (int16_t *)calloc(NB_SAMPLES, sizeof(int16_t));
    if (NULL == priv->in_buf) {
        ret = AEERROR_NOMEM;
        goto end;
    }
    priv->out_buf = (int16_t *)calloc(NB_SAMPLES, sizeof(int16_t));
    if (NULL == priv->out_buf) {
        ret = AEERROR_NOMEM;
        goto end;
    }
    priv->sdl_mutex = sdl_mutex_create();
    if (NULL == priv->sdl_mutex) {
        ret = AEERROR_NOMEM;
        goto end;
    }
    priv->is_noise_suppression_on = false;

end:
    if (ret < 0) noise_suppression_close(ctx);
    return ret;
}

static int noise_suppression_set(EffectContext *ctx,
        const char *key, int flags) {
    if(NULL == ctx) return -1;

    priv_t *priv = ctx->priv;
    AEDictionaryEntry *entry = ae_dict_get(ctx->options, key, NULL, flags);
    if (entry) {
        LogInfo("%s key = %s val = %s\n", __func__, entry->key, entry->value);
        sdl_mutex_lock(priv->sdl_mutex);
        if (0 == strcasecmp(entry->key, "Switch")) {
            if (0 == strcasecmp(entry->value, "Off")) {
                priv->is_noise_suppression_on = false;
            } else if (0 == strcasecmp(entry->value, "On")) {
                priv->is_noise_suppression_on = true;
            }
        }
        sdl_mutex_unlock(priv->sdl_mutex);
    }
    return 0;
}

static int noise_suppression_send(EffectContext *ctx,
        const void *samples, const size_t nb_samples) {
    if(!ctx) return -1;
    priv_t *priv = (priv_t *)ctx->priv;
    if(!priv || !priv->fifo_in) return -1;

    return fifo_write(priv->fifo_in, samples, nb_samples);
}

static int noise_suppression_receive(EffectContext *ctx,
        void *samples, const size_t max_nb_samples) {
    int ret = -1;
    if(!ctx) return ret;
    priv_t *priv = (priv_t *)ctx->priv;
    if(!priv || !priv->fifo_out) return ret;

    sdl_mutex_lock(priv->sdl_mutex);
    if (priv->is_noise_suppression_on) {
        while (fifo_occupancy(priv->fifo_in) > 0) {
            ret = fifo_read(priv->fifo_in, priv->in_buf, NB_SAMPLES);
            if (ret > 0) {
                ret = XmNS_Process(priv->ns, priv->in_buf, ret,
                    priv->out_buf, NB_SAMPLES);
                if (ret > 0)
                    fifo_write(priv->fifo_out, priv->out_buf, ret);
            }
        }
    } else {
        while (fifo_occupancy(priv->fifo_in) > 0) {
            size_t nb_samples =
                fifo_read(priv->fifo_in, priv->out_buf, NB_SAMPLES);
            fifo_write(priv->fifo_out, priv->out_buf, nb_samples);
        }
    }
    sdl_mutex_unlock(priv->sdl_mutex);

    if (atomic_load(&ctx->return_max_nb_samples) &&
        fifo_occupancy(priv->fifo_out) < max_nb_samples) return 0;

    return fifo_read(priv->fifo_out, samples, max_nb_samples);
}

const EffectHandler *effect_noise_suppression_fn(void) {
    static EffectHandler handler = {.name = "noise_suppression",
                                    .usage = "",
                                    .priv_size = sizeof(priv_t),
                                    .init = noise_suppression_init,
                                    .set = noise_suppression_set,
                                    .send = noise_suppression_send,
                                    .receive = noise_suppression_receive,
                                    .close = noise_suppression_close};
    return &handler;
}
