#ifndef XM_AUDIO_EFFECTS_H_
#define XM_AUDIO_EFFECTS_H_

#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include "tools/fifo.h"
#include "effects/effect_struct.h"
#include "codec/idecoder.h"
#include "effects/voice_effect.h"

enum BuffersType {
    RawPcm = 0,
    EffectsPcm,
    FifoPcm,
    NB_BUFFERS
};

enum EffectType {
    NoiseSuppression = 0,
    Beautify,
    Minions,
    VoiceMorph,
    Reverb,
    VolumeLimiter,
    MAX_NB_EFFECTS
};

typedef struct XmEffectContext {
    volatile bool flush;
    int dst_channels;
    short *buffer[NB_BUFFERS];
    EffectContext *effects[MAX_NB_EFFECTS];
    IAudioDecoder *decoder;
    fifo *audio_fifo;
} XmEffectContext;

/**
 * @brief free XmEffectContext
 *
 * @param ctx
 */
void audio_effect_freep(XmEffectContext **ctx);

/**
 * @brief Get frame data with voice effects
 *
 * @param ctx XmEffectContext
 * @param buffer buffer for storing data
 * @param buffer_size_in_short buffer size
 * @return size of valid buffer obtained.
                Less than or equal to 0 means failure or end
 */
int audio_effect_get_frame(XmEffectContext *ctx,
    short *buffer, int buffer_size_in_short);

/**
 * @brief init XmEffectContext
 *
 * @param ctx XmEffectContext
 * @param decoder Audio decoder, used to obtain raw pcm data
 * @param effects_info audio special effects info
 * @param dst_channels out number of channels
 * @return Less than 0 means failure
 */
int audio_effect_init(XmEffectContext *ctx,
    IAudioDecoder *decoder, char **effects_info, int dst_channels);

/**
 * @brief create XmEffectContext
 *
 * @return XmEffectContext*
 */
XmEffectContext *audio_effect_create();

#endif  // XM_AUDIO_EFFECTS_H_
