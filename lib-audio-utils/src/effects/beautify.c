#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "beautify/compressor.h"
#include "beautify/equalizer.h"
#include "beautify/flanger.h"
#include "beautify/limiter.h"
#include "beautify/multiband_compressor.h"
#include "effect_struct.h"
#include "error_def.h"
#include "log.h"
#include "tools/conversion.h"
#include "tools/fifo.h"
#include "tools/sdl_mutex.h"
#include "tools/util.h"

#define SAMPLE_RATE 44100
typedef struct {
    fifo *fifo_in;
    fifo *fifo_out;
    SdlMutex *sdl_mutex;
    bool is_beautify_on;
    float flp_buffer[MAX_NB_SAMPLES];
    short fix_buffer[MAX_NB_SAMPLES];
    Equalizer *equalizer;
    Compressor *compressor;
    MulCompressor *mul_compressor;
    Flanger *flanger;
    Limiter *limiter;
} priv_t;

static int beautify_close(EffectContext *ctx) {
    LogInfo("%s.\n", __func__);
    assert(NULL != ctx);

    if (ctx->priv) {
        priv_t *priv = (priv_t *)ctx->priv;
        if (priv->fifo_in) fifo_delete(&priv->fifo_in);
        if (priv->fifo_out) fifo_delete(&priv->fifo_out);
        if (priv->sdl_mutex) sdl_mutex_free(&priv->sdl_mutex);
        if (priv->equalizer) EqualizerFree(&priv->equalizer);
        if (priv->compressor) CompressorFree(&priv->compressor);
        if (priv->mul_compressor) MulCompressorFree(&priv->mul_compressor);
        if (priv->flanger) FlangerFree(&priv->flanger);
        if (priv->limiter) LimiterFree(&priv->limiter);
    }
    return 0;
}

static int beautify_init(EffectContext *ctx, int argc, const char **argv) {
    LogInfo("%s.\n", __func__);
    for (int i = 0; i < argc; ++i) {
        LogInfo("argv[%d] = %s\n", i, argv[i]);
    }
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    if (NULL == priv) return AEERROR_NULL_POINT;

    int ret = 0;
    priv->equalizer = EqualizerCreate(SAMPLE_RATE);
    if (NULL == priv->equalizer) {
        ret = AEERROR_NOMEM;
        goto end;
    }

    priv->compressor = CompressorCreate(SAMPLE_RATE);
    if (NULL == priv->compressor) {
        ret = AEERROR_NOMEM;
        goto end;
    }

    priv->mul_compressor = MulCompressorCreate(SAMPLE_RATE);
    if (NULL == priv->compressor) {
        ret = AEERROR_NOMEM;
        goto end;
    }

    priv->flanger = FlangerCreate(SAMPLE_RATE);
    if (NULL == priv->flanger) {
        ret = AEERROR_NOMEM;
        goto end;
    }

    priv->limiter = LimiterCreate(SAMPLE_RATE);
    if (NULL == priv->limiter) {
        ret = AEERROR_NOMEM;
        goto end;
    }
    LimiterSetSwitch(priv->limiter, 1);
    LimiterSet(priv->limiter, -0.5f, 0.0f, 0.0f, 0.0f);

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
    priv->is_beautify_on = false;

end:
    if (ret < 0) beautify_close(ctx);
    return ret;
}

// 清晰人声
static void create_clean_voice(priv_t *priv) {
    EqualizerSetMode(priv->equalizer, EqCleanVoice);
    CompressorSet(priv->compressor, -19.8f, 3.01f, 2.0f, 50.0f, 5.5f);
    MulCompressorSetMode(priv->mul_compressor, MulComCleanVoice);
    FlangerSet(priv->flanger, 0.0f, 0.0f, 0.0f, 0.0f, 0.1f, WAVE_SINE, 0.0f);
}

// 低音
static void create_bass_effect(priv_t *priv) {
    // 低音 -> 沉稳
    EqualizerSetMode(priv->equalizer, EqBass);
    CompressorSet(priv->compressor, -24.2f, 5.0f, 3.76f, 50.0f, 5.7f);
    MulCompressorSetMode(priv->mul_compressor, MulComBass);
    FlangerSet(priv->flanger, 0.0f, 0.0f, 0.0f, 0.0f, 0.1f, WAVE_SINE, 0.0f);
}
// 低沉
static void create_low_voice(priv_t *priv) {
    // 低沉 -> 低音
    EqualizerSetMode(priv->equalizer, EqLowVoice);
    CompressorSet(priv->compressor, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f);
    MulCompressorSetMode(priv->mul_compressor, MulComLowVoice);
    FlangerSet(priv->flanger, 0.0f, 0.0f, 0.0f, 0.0f, 0.1f, WAVE_SINE, 0.0f);
}

// 穿透
static void create_penetrating_effect(priv_t *priv) {
    // 穿透
    EqualizerSetMode(priv->equalizer, EqPenetrating);
    CompressorSet(priv->compressor, -21.0f, 3.22f, 1.0f, 40.0f, 9.0f);
    MulCompressorSetMode(priv->mul_compressor, MulComPenetrating);
    FlangerSet(priv->flanger, 0.0f, 0.0f, 0.0f, 0.0f, 0.1f, WAVE_SINE, 0.0f);
}

// 磁性
static void create_magnetic_effect(priv_t *priv) {
    // 磁性
    EqualizerSetMode(priv->equalizer, EqMagnetic);
    CompressorSet(priv->compressor, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f);
    MulCompressorSetMode(priv->mul_compressor, MulComMagnetic);
    FlangerSet(priv->flanger, 0.0f, 2.0f, 10.0f, 20.0f, 1.5f, WAVE_SINE, 25.0f);
}

// 柔和高音
static void create_soft_pitch(priv_t *priv) {
    // 柔和高音
    EqualizerSetMode(priv->equalizer, EqSoftPitch);
    CompressorSet(priv->compressor, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f);
    MulCompressorSetMode(priv->mul_compressor, MulComSoftPitch);
    FlangerSet(priv->flanger, 0.0f, 0.0f, 0.0f, 0.0f, 0.1f, WAVE_SINE, 0.0f);
}

static void beautify_set_mode(priv_t *priv, const char *mode) {
    LogInfo("%s mode = %s.\n", __func__, mode);
    sdl_mutex_lock(priv->sdl_mutex);

    priv->is_beautify_on = true;

    if (0 == strcasecmp(mode, "None")) {
        priv->is_beautify_on = false;
    } else if (0 == strcasecmp(mode, "CleanVoice")) {
        create_clean_voice(priv);
    } else if (0 == strcasecmp(mode, "Bass")) {
        create_bass_effect(priv);
    } else if (0 == strcasecmp(mode, "LowVoice")) {
        create_low_voice(priv);
    } else if (0 == strcasecmp(mode, "Penetrating")) {
        create_penetrating_effect(priv);
    } else if (0 == strcasecmp(mode, "Magnetic")) {
        create_magnetic_effect(priv);
    } else if (0 == strcasecmp(mode, "SoftPitch")) {
        create_soft_pitch(priv);
    } else {
        priv->is_beautify_on = false;
    }

    sdl_mutex_unlock(priv->sdl_mutex);
}

static int beautify_set(EffectContext *ctx, const char *key, int flags) {
    LogInfo("%s key = %s.\n", __func__, key);
    AEDictionaryEntry *entry = ae_dict_get(ctx->options, key, NULL, flags);
    if (entry) {
        if (0 == strcasecmp(entry->key, "mode")) {
            beautify_set_mode(ctx->priv, entry->value);
        }
    }
    return 0;
}

static int beautify_send(EffectContext *ctx, const void *samples,
                         const size_t nb_samples) {
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    assert(NULL != priv);
    assert(NULL != priv->fifo_in);

    return fifo_write(priv->fifo_in, samples, nb_samples);
}

static int beautify_receive(EffectContext *ctx, void *samples,
                            const size_t max_nb_samples) {
    assert(NULL != ctx);
    priv_t *priv = (priv_t *)ctx->priv;
    assert(NULL != priv);
    assert(NULL != priv->fifo_in);

    sdl_mutex_lock(priv->sdl_mutex);
    if (priv->is_beautify_on) {
        int nb_samples =
            fifo_read(priv->fifo_in, priv->fix_buffer, MAX_NB_SAMPLES);
        while (nb_samples > 0) {
            S16ToFloat(priv->fix_buffer, priv->flp_buffer, nb_samples);
            // 均衡器处理
            EqualizerProcess(priv->equalizer, priv->flp_buffer, nb_samples);
            // 单频段压缩器处理
            nb_samples = CompressorProcess(priv->compressor, priv->flp_buffer,
                                           nb_samples);
            // 多频段压缩器处理
            nb_samples = MulCompressorProcess(priv->mul_compressor,
                                              priv->flp_buffer, nb_samples);
            // 镶边处理
            FlangerProcess(priv->flanger, priv->flp_buffer, nb_samples);
            // 限制器处理
            nb_samples =
                LimiterProcess(priv->limiter, priv->flp_buffer, nb_samples);
            FloatToS16(priv->flp_buffer, priv->fix_buffer, nb_samples);
            fifo_write(priv->fifo_out, priv->fix_buffer, nb_samples);
            nb_samples =
                fifo_read(priv->fifo_in, priv->fix_buffer, MAX_NB_SAMPLES);
        }
    } else {
        while (fifo_occupancy(priv->fifo_in) > 0) {
            int nb_samples = fifo_read(priv->fifo_in, samples, max_nb_samples);
            fifo_write(priv->fifo_out, samples, nb_samples);
        }
    }

    sdl_mutex_unlock(priv->sdl_mutex);

    if (atomic_load(&ctx->return_max_nb_samples) &&
        fifo_occupancy(priv->fifo_out) < max_nb_samples)
        return 0;
    return fifo_read(priv->fifo_out, samples, max_nb_samples);
}

const EffectHandler *effect_beautify_fn(void) {
    static EffectHandler handler = {.name = "beautify",
                                    .usage = "",
                                    .priv_size = sizeof(priv_t),
                                    .init = beautify_init,
                                    .set = beautify_set,
                                    .send = beautify_send,
                                    .receive = beautify_receive,
                                    .close = beautify_close};
    return &handler;
}
