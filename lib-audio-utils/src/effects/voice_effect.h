//
// Created by layne on 19-4-27.
//

#ifndef VOICE_EFFECT_H_
#define VOICE_EFFECT_H_

#include "effect_struct.h"

const EffectHandler *find_effect(char const *name);
EffectContext *create_effect(const EffectHandler *handler,
                             const int sample_rate, const int channels);
const char *show_usage(EffectContext *ctx);
int init_effect(EffectContext *ctx, int argc, const char **argv);
int set_effect(EffectContext *ctx, const char *key, const char *value,
               int flags);
int send_samples(EffectContext *ctx, const void *samples,
                 const size_t nb_samples);
int receive_samples(EffectContext *ctx, void *samples,
                    const size_t max_nb_samples);
void free_effect(EffectContext *ctx);

#endif  // AUDIO_EFFECTS_H_
