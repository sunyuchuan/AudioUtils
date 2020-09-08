#include <assert.h>
#include <stdio.h>
#include "effect_struct.h"
#include "error_def.h"
#include "log.h"
#include "resample/resample.h"
#include "tables/recip_table.h"
#include "tools/fifo.h"
#include "tools/sdl_mutex.h"
#include "tools/util.h"

#define SRC_SAMPLE_RATE 44100
#define DST_SAMPLE_RATE 25200
#define RESAMPLE_FRAME_LEN 512

typedef struct SolaT {
    short analysis_window_offset;
    short synthesis_window_offset;
    short min_overlap;
    short opt_overlap_pos;
    short Lmax;
    short in_len;
    short out_len;
    int16_t *input;
    int16_t *output;
    int16_t *frame_update;
} Sola;

typedef struct {
    fifo *fifo_in;
    fifo *fifo_out;
    fifo *fifo_swr;
    SdlMutex *sdl_mutex;
    Sola *sola;
    struct SwrContext *swr_ctx;
    uint8_t **src_samples;
    uint8_t **dst_samples;
    int max_dst_nb_samples;
    bool is_minions_on;
} priv_t;

static void sola_free(Sola **sola) {
    if (NULL == sola || NULL == *sola) return;
    Sola *self = *sola;
    if (self->input) {
        free(self->input);
        self->input = NULL;
    }
    if (self->output) {
        free(self->output);
        self->output = NULL;
    }
    if (self->frame_update) {
        free(self->frame_update);
        self->frame_update = NULL;
    }
    free(*sola);
    *sola = NULL;
}

static int sola_init(Sola *sola, short pitch_sample, float tsm_factor) {
    sola->Lmax = pitch_sample * 1.5;
    float tmp = tsm_factor - 1.0f;
    tmp = tmp > 0.0f ? tmp : -tmp;

    sola->analysis_window_offset = 1.0f / tmp * (pitch_sample >> 1) + 0.5f;
    sola->synthesis_window_offset =
        sola->analysis_window_offset * tsm_factor + 0.5f;
    sola->min_overlap = sola->Lmax - pitch_sample;
    sola->in_len = sola->Lmax + sola->synthesis_window_offset;
    sola->out_len = sola->Lmax << 1;
    // 分配空间
    sola->input = (int16_t *)calloc(sola->in_len, sizeof(int16_t));
    sola->output = (int16_t *)calloc(sola->out_len, sizeof(int16_t));
    sola->frame_update =
        (int16_t *)calloc(sola->analysis_window_offset, sizeof(int16_t));

    if (NULL == sola->input || NULL == sola->output ||
        NULL == sola->frame_update)
        return AEERROR_NOMEM;
    return 0;
}

static void sola_inframe_update(Sola *sola) {
    memmove(sola->input, sola->input + sola->analysis_window_offset,
            sizeof(int16_t) * (sola->in_len - sola->analysis_window_offset));
    memcpy(sola->input + sola->in_len - sola->analysis_window_offset,
           sola->frame_update, sizeof(int16_t) * sola->analysis_window_offset);
}

static void sola_outframe_update(Sola *sola) {
    memmove(sola->output, sola->output + sola->synthesis_window_offset,
            sizeof(int16_t) * sola->Lmax);
}

static void sola_porcess(Sola *sola) {
    int mVal = 0;
    short xcorr_len = sola->Lmax - sola->min_overlap - 1;
    short *seg1 = sola->output + xcorr_len;
    short *seg2 = sola->input;
    short prdt_len = sola->min_overlap + 1;
    short index = 0;

    /*cross-correlation*/
    for (short i = 0; i < xcorr_len; i++) {
        short *in1 = seg1;
        short *in2 = seg2;
        int sum = 0;
        for (short j = 0; j < prdt_len; j++) {
            sum += (*in1++ * *in2++) >> 4;  // Q26,0.0000000149
        }
        prdt_len++;
        seg1--;

        if (sum > mVal) {
            mVal = sum;
            index = i;
        }
    }

    if (mVal == 0) {
        sola->opt_overlap_pos = sola->Lmax;
    } else {
        sola->opt_overlap_pos = sola->min_overlap + index + 1;
    }
}

static void sola_linear_cross_fade(Sola *sola) {
    short cross_fade_duration = sola->opt_overlap_pos;
    short index = cross_fade_duration - 1;
    short Recip = RecipTab[index - sola->min_overlap];
    short *seg1 = sola->output + sola->Lmax - cross_fade_duration;
    short *seg2 = sola->input;

    if (cross_fade_duration != 1) {
        short linear_const1 = index * Recip;
        for (short i = 0; i < cross_fade_duration; i++) {
            short delta = i * Recip;
            int tmp = *seg1 * (linear_const1 - delta);
            tmp += *seg2++ * delta;
            *seg1++ = tmp >> 15;
        }
    }

    seg1 = sola->output + sola->Lmax;
    seg2 = sola->input + cross_fade_duration;
    for (short i = 0; i < sola->synthesis_window_offset; i++) {
        *seg1++ = *seg2++;
    }
}

static int minions_close(EffectContext *ctx) {
    LogInfo("%s.\n", __func__);
    assert(NULL != ctx);

    if (ctx->priv) {
        priv_t *priv = (priv_t *)ctx->priv;
        if (priv->fifo_in) fifo_delete(&priv->fifo_in);
        if (priv->fifo_out) fifo_delete(&priv->fifo_out);
        if (priv->fifo_swr) fifo_delete(&priv->fifo_swr);
        if (priv->sdl_mutex) sdl_mutex_free(&priv->sdl_mutex);
        if (priv->sola) sola_free(&priv->sola);
        if (priv->swr_ctx) swr_free(&priv->swr_ctx);
        if (priv->src_samples) {
            av_freep(&priv->src_samples[0]);
            av_freep(&priv->src_samples);
        }
        if (priv->dst_samples) {
            av_freep(&priv->dst_samples[0]);
            av_freep(&priv->dst_samples);
        }
    }
    return 0;
}

static int minions_init(EffectContext *ctx, int argc, const char **argv) {
    LogInfo("%s.\n", __func__);
    for (int i = 0; i < argc; ++i) {
        LogInfo("argv[%d] = %s\n", i, argv[i]);
    }
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    if (NULL == priv) return AEERROR_NULL_POINT;

    int ret = 0;
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
    priv->fifo_swr = fifo_create(sizeof(int16_t));
    if (NULL == priv->fifo_swr) {
        ret = AEERROR_NOMEM;
        goto end;
    }
    priv->sdl_mutex = sdl_mutex_create();
    if (NULL == priv->sdl_mutex) {
        ret = AEERROR_NOMEM;
        goto end;
    }
    priv->sola = (Sola *)calloc(1, sizeof(Sola));
    if (NULL == priv->sola) {
        ret = AEERROR_NOMEM;
        goto end;
    }
    priv->is_minions_on = false;
    priv->max_dst_nb_samples = RESAMPLE_FRAME_LEN;
    ret = sola_init(priv->sola, 400, 1.75f);
    ret = resampler_init(1, 1, SRC_SAMPLE_RATE, DST_SAMPLE_RATE,
                         AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_S16, &priv->swr_ctx);
    ret = allocate_sample_buffer(&priv->src_samples, 1, RESAMPLE_FRAME_LEN,
                                 AV_SAMPLE_FMT_S16);
    ret = allocate_sample_buffer(&priv->dst_samples, 1,
                                 priv->max_dst_nb_samples, AV_SAMPLE_FMT_S16);

end:
    if (ret < 0) minions_close(ctx);
    return ret;
}

static int minions_set(EffectContext *ctx, const char *key, int flags) {
    assert(NULL != ctx);

    priv_t *priv = ctx->priv;
    AEDictionaryEntry *entry = ae_dict_get(ctx->options, key, NULL, flags);
    if (entry) {
        LogInfo("%s key = %s val = %s\n", __func__, entry->key, entry->value);

        sdl_mutex_lock(priv->sdl_mutex);
        if (0 == strcasecmp(entry->key, "Switch")) {
            if (0 == strcasecmp(entry->value, "On")) {
                priv->is_minions_on = true;
            } else if (0 == strcasecmp(entry->value, "Off")) {
                priv->is_minions_on = false;
            }
        }
        sdl_mutex_unlock(priv->sdl_mutex);
    }
    return 0;
}

static int minions_send(EffectContext *ctx, const void *samples,
                        const size_t nb_samples) {
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    assert(NULL != priv);
    assert(NULL != priv->fifo_in);

    return fifo_write(priv->fifo_in, samples, nb_samples);
}

static int minions_receive(EffectContext *ctx, void *samples,
                           const size_t max_nb_samples) {
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    assert(NULL != priv);
    assert(NULL != priv->fifo_in);

    if (priv->is_minions_on) {
        sdl_mutex_lock(priv->sdl_mutex);
        while (fifo_occupancy(priv->fifo_in) >=
               (size_t)priv->sola->analysis_window_offset) {
            fifo_read(priv->fifo_in, priv->sola->frame_update,
                      priv->sola->analysis_window_offset);
            sola_inframe_update(priv->sola);
            sola_porcess(priv->sola);
            sola_linear_cross_fade(priv->sola);
            fifo_write(priv->fifo_swr, priv->sola->output,
                       priv->sola->synthesis_window_offset);
            sola_outframe_update(priv->sola);
        }
        while (fifo_occupancy(priv->fifo_swr) >= RESAMPLE_FRAME_LEN) {
            fifo_read(priv->fifo_swr, priv->src_samples[0], RESAMPLE_FRAME_LEN);
            int ret = resample_audio(priv->swr_ctx, priv->src_samples,
                                     RESAMPLE_FRAME_LEN, &priv->dst_samples,
                                     &priv->max_dst_nb_samples, 1);
            if (ret > 0) fifo_write(priv->fifo_out, priv->dst_samples[0], ret);
        }
        sdl_mutex_unlock(priv->sdl_mutex);
    } else {
        while (fifo_occupancy(priv->fifo_in) > 0) {
            size_t nb_samples =
                fifo_read(priv->fifo_in, samples, max_nb_samples);
            fifo_write(priv->fifo_swr, samples, nb_samples);
        }
        while (fifo_occupancy(priv->fifo_swr) > 0) {
            size_t nb_samples =
                fifo_read(priv->fifo_swr, samples, max_nb_samples);
            fifo_write(priv->fifo_out, samples, nb_samples);
        }
    }

    if (atomic_load(&ctx->return_max_nb_samples) &&
        fifo_occupancy(priv->fifo_out) < max_nb_samples)
        return 0;
    return fifo_read(priv->fifo_out, samples, max_nb_samples);
}

const EffectHandler *effect_minions_fn(void) {
    static EffectHandler handler = {.name = "minions",
                                    .usage = "",
                                    .priv_size = sizeof(priv_t),
                                    .init = minions_init,
                                    .set = minions_set,
                                    .send = minions_send,
                                    .receive = minions_receive,
                                    .close = minions_close};
    return &handler;
}
