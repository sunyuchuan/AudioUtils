#ifndef _AUDIO_SOURCE_H_
#define _AUDIO_SOURCE_H_
#include "mixer/fade_in_out.h"
#include "codec/idecoder.h"
#include "effects/xm_audio_effects.h"

struct TrackBuffer {
    bool mute;
    short *buffer;
};

typedef struct AudioSource {
    // source file type
    bool is_pcm;
    // loop source
    bool is_loop;
    // pcm file parameters
    int sample_rate;
    int nb_channels;
    // cutting parameters
    int crop_start_time_ms;
    int crop_end_time_ms;
    // source location
    int start_time_ms;
    int end_time_ms;
    // source volume
    float volume;
    // mixing parameters
    float left_factor;
    float right_factor;
    // side chain parameters
    float yl_prev;
    float makeup_gain;
    bool side_chain_enable;
    // source address
    char *file_path;
    // source decoder
    IAudioDecoder *decoder;
    // effect parameters
    XmEffectContext *effects_ctx;
    bool has_effects;
    char *effects_info[MAX_NB_EFFECTS];
    struct TrackBuffer buffer;
    // fade in and fade out parameters
    FadeInOut fade_io;
} AudioSource;

void AudioSource_free(AudioSource *source);
void AudioSource_freep(AudioSource **source);
#endif
