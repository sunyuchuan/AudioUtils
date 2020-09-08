#ifndef _MIXER_EFFECTS_
#define _MIXER_EFFECTS_
#include "source/audio_source_queue.h"

// Limit the maximum duration of a mix
//#define MAX_DURATION_MIX_IN_MS (50*60*1000)
#define MAX_NB_TRACKS 5

typedef struct MixerEffects {
    int duration_ms;
    int nb_tracks;
    AudioSource *source[MAX_NB_TRACKS];
    AudioSourceQueue *sourceQueue[MAX_NB_TRACKS];
    AudioSourceQueue *sourceQueueBackup[MAX_NB_TRACKS];
} MixerEffects;

#endif
