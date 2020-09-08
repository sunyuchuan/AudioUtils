#ifndef _AUDIO_SOURCE_QUEUE_H_
#define _AUDIO_SOURCE_QUEUE_H_
#include "audio_source.h"
#include <pthread.h>

typedef struct AudioSourceList {
    AudioSource source;
    struct AudioSourceList *next;
} AudioSourceList;

typedef struct AudioSourceQueue
{
    AudioSourceList *mFirst;
    AudioSourceList *mLast;
    volatile int mNumbers;
    pthread_mutex_t mLock;
} AudioSourceQueue;

void AudioSourceQueue_copy(AudioSourceQueue *src, AudioSourceQueue *dst);
int AudioSourceQueue_get_end_time_ms(AudioSourceQueue *queue);
void AudioSourceQueue_bubble_sort(AudioSourceQueue *queue);
int AudioSourceQueue_size(AudioSourceQueue *queue);
void AudioSourceQueue_flush(AudioSourceQueue *queue);
void AudioSourceQueue_free(AudioSourceQueue *queue);
void AudioSourceQueue_freep(AudioSourceQueue **queue);
int AudioSourceQueue_get(AudioSourceQueue *queue, AudioSource *source);
int AudioSourceQueue_put(AudioSourceQueue *queue, AudioSource *source);
AudioSourceQueue *AudioSourceQueue_create();

#endif
