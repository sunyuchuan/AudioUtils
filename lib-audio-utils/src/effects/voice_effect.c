//
// Created by layne on 19-4-27.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "voice_effect.h"
#include "error_def.h"
#if defined(__ANDROID__) || defined (__linux__)
#include "libavutil/mem.h"
#else
#include "tools/mem.h"
#endif
#include "tools/util.h"

#define EFFECT(f) extern const EffectHandler *effect_##f##_fn(void);
#include "effects.h"
#undef EFFECT

static effect_fn all_effect_fns[] = {
#define EFFECT(f) effect_##f##_fn,
#include "effects.h"
#undef EFFECT
    NULL};

const effect_fn *get_all_effect_fns(void) { return all_effect_fns; }

const EffectHandler *find_effect(char const *name) {
    const effect_fn *fns = get_all_effect_fns();
    for (int i = 0; NULL != fns[i]; ++i) {
        const EffectHandler *eh = fns[i]();
        if (eh && eh->name && strcasecmp(eh->name, name) == 0) return eh;
    }
    return NULL;
}

EffectContext *create_effect(const EffectHandler *handler,
                             const int sample_rate, const int channels) {
    if (NULL == handler) return NULL;
    EffectContext *self = (EffectContext *)calloc(1, sizeof(EffectContext));
    atomic_store(&self->return_max_nb_samples, false);
    self->handler = *handler;
    self->in_signal.sample_rate = sample_rate;
    self->in_signal.channels = channels;
    self->priv = av_mallocz(handler->priv_size);
    return self;
}

const char *show_usage(EffectContext *ctx) {
    assert(NULL != ctx);
    return ctx->handler.usage;
}

int init_effect(EffectContext *ctx, int argc, const char **argv) {
    if(NULL == ctx) {
        return -1;
    }

    return ctx->handler.init(ctx, argc, argv);
}

int set_effect(EffectContext *ctx, const char *key, const char *value,
               int flags) {
    if(NULL == ctx) {
        return -1;
    }

    if (0 == strcasecmp(key, "return_max_nb_samples")) {
        if (0 == strcasecmp(value, "True"))
            atomic_store(&ctx->return_max_nb_samples, true);
        else
            atomic_store(&ctx->return_max_nb_samples, false);
    }
    ae_dict_set(&ctx->options, key, value, flags);
    return ctx->handler.set(ctx, key, flags);
}

int send_samples(EffectContext *ctx, const void *samples,
                 const size_t nb_samples) {
    if(NULL == ctx) {
        return -1;
    }

    return ctx->handler.send(ctx, samples, nb_samples);
}

int receive_samples(EffectContext *ctx, void *samples,
                    const size_t max_nb_samples) {
    if(NULL == ctx) {
        return -1;
    }

    return ctx->handler.receive(ctx, samples, max_nb_samples);
}

void free_effect(EffectContext *ctx) {
    if (NULL == ctx) return;
    ctx->handler.close(ctx);
    if (ctx->priv) av_freep(&ctx->priv);
    if (ctx->options) ae_dict_free(&ctx->options);
    free(ctx);
}
