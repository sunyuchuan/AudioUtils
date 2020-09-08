#include "audio_source.h"
#include <stdlib.h>
#include <string.h>
#include "effects/voice_effect.h"

void AudioSource_free(AudioSource *source) {
    if (source) {
        if (source->effects_ctx) {
            audio_effect_freep(&source->effects_ctx);
        }

        for (short i = 0; i < MAX_NB_EFFECTS; ++i) {
            if (source->effects_info[i]) {
                free(source->effects_info[i]);
                source->effects_info[i] = NULL;
            }
        }

        if (source->file_path) {
            free(source->file_path);
            source->file_path = NULL;
        }

        if (source->decoder) {
            IAudioDecoder_freep(&(source->decoder));
        }

        if (source->buffer.buffer) {
            free(source->buffer.buffer);
            source->buffer.buffer = NULL;
        }

        memset(source, 0, sizeof(AudioSource));
    }
}

void AudioSource_freep(AudioSource **source) {
    if (source && *source) {
        AudioSource_free(*source);
        free(*source);
        *source = NULL;
    }
}
