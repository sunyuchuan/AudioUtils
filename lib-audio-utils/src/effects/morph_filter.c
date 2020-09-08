#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "effect_struct.h"
#include "error_def.h"
#include "log.h"
#include "tools/util.h"
#include "tools/fifo.h"
#include "morph_filter/pitch_tracker/src/pitch_macro.h"
#include "morph_filter/voice_morph/morph/voice_morph.h"

#define NB_SAMPLES 1024
#define OUTPUT_BUF_SIZE (1024 << 3)

typedef struct {
    VoiceMorph *morph;
    bool robot;
    fifo *fifo_in;
    fifo *fifo_out;
    int16_t *in_buf;
    int16_t *out_buf;
    bool is_morph_on;
    pthread_mutex_t mutex;
} priv_t;

enum MorphType { NONE_MORPH = 0, ROBOT, BRIGHT, MAN, WOMAN };

static void morph_core_free(VoiceMorph **morph) {
    if (!morph || !(*morph)) return;

    VoiceMorph_Release(morph);
}

static int voice_morph_close(EffectContext *ctx) {
    assert(NULL != ctx);
    if (NULL == ctx->priv) return 0;
    priv_t *priv = (priv_t *)ctx->priv;
    if (priv->in_buf) {
        free(priv->in_buf);
        priv->in_buf = NULL;
    }
    if (priv->out_buf) {
        free(priv->out_buf);
        priv->out_buf = NULL;
    }
    if (priv->fifo_in) fifo_delete(&priv->fifo_in);
    if (priv->fifo_out) fifo_delete(&priv->fifo_out);
    if (priv->morph) morph_core_free(&priv->morph);
    pthread_mutex_destroy(&priv->mutex);
    return 0;
}

static int voice_morph_init(EffectContext *ctx, int argc, const char **argv) {
    LogInfo("%s.\n", __func__);
    for (int i = 0; i < argc; ++i) {
        LogInfo("argv[%d] = %s\n", i, argv[i]);
    }
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    assert(NULL != priv);

    int ret = 0;

    priv->in_buf = (int16_t *)calloc(NB_SAMPLES, sizeof(int16_t));
    if (NULL == priv->in_buf) {
        ret = AEERROR_NOMEM;
        goto end;
    }
    priv->out_buf = (int16_t *)calloc(OUTPUT_BUF_SIZE, sizeof(int16_t));
    if (NULL == priv->out_buf) {
        ret = AEERROR_NOMEM;
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

    priv->morph = VoiceMorph_Create(NULL);
    if (NULL == priv->morph) {
        ret = -1;
        goto end;
    }

    ret = VoiceMorph_Init(priv->morph);
    if (ret < 0) goto end;

    priv->is_morph_on = false;
    pthread_mutex_init(&priv->mutex, NULL);

end:
    if (ret < 0) voice_morph_close(ctx);
    return ret;
}

static int morph_core_set_type(priv_t *priv,
        enum MorphType type) {
    float pitch_coeff = 1.0f;
    int ret = -1;
    switch (type) {
        case NONE_MORPH:
            pitch_coeff = 1.0f;
            priv->robot = false;
            break;
        case ROBOT:
            pitch_coeff = 1.0f;
            priv->robot = true;
            break;
        case BRIGHT:
            pitch_coeff = 1.0f;
            priv->robot = false;
            break;
        case MAN:
            pitch_coeff = 0.8f;
            priv->robot = false;
            break;
        case WOMAN:
            pitch_coeff = 1.5f;
            priv->robot = false;
            break;
        default:
            break;
    }

    pthread_mutex_lock(&priv->mutex);
    ret = VoiceMorph_SetConfig(priv->morph, pitch_coeff);
    pthread_mutex_unlock(&priv->mutex);
    return ret;
}

static void voice_morph_set_mode(priv_t *priv, const char *mode) {
    if (0 == strcasecmp(mode, "None")) {
        LogInfo("%s set original.\n", __func__);
        morph_core_set_type(priv, NONE_MORPH);
        priv->is_morph_on = false;
    } else if (0 == strcasecmp(mode, "bright")) {
        LogInfo("%s set bright.\n", __func__);
        morph_core_set_type(priv, BRIGHT);
        priv->is_morph_on = false;
    } else if (0 == strcasecmp(mode, "robot")) {
        LogInfo("%s set robot.\n", __func__);
        morph_core_set_type(priv, ROBOT);
        priv->is_morph_on = true;
    } else if (0 == strcasecmp(mode, "man")) {
        LogInfo("%s set man.\n", __func__);
        morph_core_set_type(priv, MAN);
        priv->is_morph_on = true;
    } else if (0 == strcasecmp(mode, "woman")) {
        LogInfo("%s set woman.\n", __func__);
        morph_core_set_type(priv, WOMAN);
        priv->is_morph_on = true;
    }
}

static int voice_morph_set(EffectContext *ctx, const char *key, int flags) {
    assert(NULL != ctx);

    AEDictionaryEntry *entry = ae_dict_get(ctx->options, key, NULL, flags);
    if (entry) {
        LogInfo("%s key = %s val = %s\n", __func__, entry->key, entry->value);
        if (0 == strcasecmp(entry->key, "mode")) {
            voice_morph_set_mode((priv_t *)ctx->priv, entry->value);
        } else if (0 == strcasecmp(entry->key, "Switch")) {
            if (0 == strcasecmp(entry->value, "On")) {
                ((priv_t *)ctx->priv)->is_morph_on = true;
            } else if (0 == strcasecmp(entry->value, "Off")) {
                ((priv_t *)ctx->priv)->is_morph_on = false;
            }
        }
    }
    return 0;
}

static int voice_morph_send(EffectContext *ctx, const void *samples,
                            const size_t nb_samples) {
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    assert(NULL != priv);
    assert(NULL != priv->fifo_in);

    return fifo_write(priv->fifo_in, samples, nb_samples);
}

static int voice_morph_receive(EffectContext *ctx,
        void *samples, const size_t max_nb_samples) {
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    assert(NULL != priv);
    assert(NULL != priv->morph);
    assert(NULL != priv->fifo_in);

    int ret = 0;
    if (priv->is_morph_on) {
        while (fifo_occupancy(priv->fifo_in) > 0) {
            ret = fifo_read(priv->fifo_in, priv->in_buf, NB_SAMPLES);
            int output_size = 0;
            pthread_mutex_lock(&priv->mutex);
            ret = VoiceMorph_Process(priv->morph,
                    (void*)priv->in_buf, ret << 1,
                    (char*)priv->out_buf, &output_size, priv->robot);
            pthread_mutex_unlock(&priv->mutex);
            if (ret < 0) {
                LogError("%s VoiceMorph_Process error %d.\n", __func__, ret);
                break;
            }
            fifo_write(priv->fifo_out, priv->out_buf, output_size >> 1);
        }
    } else {
        while (fifo_occupancy(priv->fifo_in) > 0) {
            size_t nb_samples =
                fifo_read(priv->fifo_in, samples, max_nb_samples);
            fifo_write(priv->fifo_out, samples, nb_samples);
        }
    }

    if (atomic_load(&ctx->return_max_nb_samples) &&
        fifo_occupancy(priv->fifo_out) < max_nb_samples)
        return 0;
    return fifo_read(priv->fifo_out, samples, max_nb_samples);
}

const EffectHandler *effect_voice_morph_fn(void) {
    static EffectHandler handler = {.name = "voice_morph",
                                    .usage = "",
                                    .priv_size = sizeof(priv_t),
                                    .init = voice_morph_init,
                                    .set = voice_morph_set,
                                    .send = voice_morph_send,
                                    .receive = voice_morph_receive,
                                    .close = voice_morph_close};
    return &handler;
}
