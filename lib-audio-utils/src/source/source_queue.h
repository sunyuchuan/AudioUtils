#include "audio_source_queue.h"
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include <limits.h>

static size_t max_alloc_size = INT_MAX;

static void *av_realloc(void *ptr, size_t size) {
    if (size > (max_alloc_size - 32)) return NULL;
    return realloc(ptr, size + !size);
}

static char *av_strdup(const char *s) {
    char *ptr = NULL;
    if (s) {
        size_t len = strlen(s) + 1;
        ptr = av_realloc(NULL, len);
        if (ptr) memcpy(ptr, s, len);
    }
    return ptr;
}

#define DECLARE_FUNC_IS_VALID(n)                            \
static bool n##_isValid(n *source)                          \
{                                                           \
    if(!source || !source->file_path)                       \
        return false;                                       \
                                                            \
    return true;                                            \
}                                                           \

#define DECLARE_FUNC_COPY(n)                                \
void n##Queue_copy(n##Queue *src, n##Queue *dst)            \
{                                                           \
    if(!src || !dst)                                        \
        return;                                             \
                                                            \
    n##List *sourceList, *next;                             \
    n source;                                               \
    n##Queue_flush(dst);                                    \
                                                            \
    for(sourceList = src->mFirst; sourceList != NULL;       \
            sourceList = next) {                            \
        pthread_mutex_lock(&src->mLock);                    \
        next = sourceList->next;                            \
        source = sourceList->source;                        \
        source.file_path =                                  \
            av_strdup(sourceList->source.file_path);        \
        for(int i = 0; i < MAX_NB_EFFECTS; ++i) {           \
            source.effects_info[i] =                        \
                av_strdup(sourceList->source.effects_info[i]);\
        }                                                   \
        source.buffer.buffer = NULL;                        \
        source.decoder = NULL;                              \
        source.effects_ctx = NULL;                          \
        pthread_mutex_unlock(&src->mLock);                  \
        n##Queue_put(dst, &source);                         \
    }                                                       \
}                                                           \

#define DECLARE_FUNC_GET_END_TIME_MS(n)                     \
int n##Queue_get_end_time_ms(n##Queue *queue)               \
{                                                           \
    if(!queue || !queue->mLast)                             \
        return -1;                                          \
                                                            \
    pthread_mutex_lock(&queue->mLock);                      \
    int end_time_ms = queue->mLast->source.end_time_ms;     \
    pthread_mutex_unlock(&queue->mLock);                    \
    return end_time_ms;                                     \
}                                                           \

#define DECLARE_FUNC_BUBBLE_SORT(n)                         \
void n##Queue_bubble_sort(n##Queue *queue)                  \
{                                                           \
    if(!queue)                                              \
        return;                                             \
    n##List *sourceList, *prev, *next;                      \
                                                            \
    pthread_mutex_lock(&queue->mLock);                      \
    for (int i = queue->mNumbers - 1; i > 0; i--) {         \
        prev = NULL;sourceList = queue->mFirst;next = NULL; \
        for (int j = 0; j < i; j++) {                       \
            next = sourceList->next;                        \
            if (sourceList->source.start_time_ms >          \
                    next->source.start_time_ms) {           \
                sourceList->next = next->next;              \
                next->next = sourceList;                    \
                if (prev) prev->next = next;                \
                else queue->mFirst = next;                  \
                prev = next;                                \
            } else {                                        \
                prev = sourceList;                          \
                sourceList = sourceList->next;              \
            }                                               \
        }                                                   \
        if (!next->next) queue->mLast = next;               \
        if (!sourceList->next) queue->mLast = sourceList;   \
    }                                                       \
    pthread_mutex_unlock(&queue->mLock);                    \
}                                                           \

#define DECLARE_FUNC_SIZE(n)                                \
int n##Queue_size(n##Queue *queue)                          \
{                                                           \
    if(!queue)                                              \
        return -1;                                          \
                                                            \
    pthread_mutex_lock(&queue->mLock);                      \
    int size = queue->mNumbers;                             \
    pthread_mutex_unlock(&queue->mLock);                    \
    return size;                                            \
}                                                           \

#define DECLARE_FUNC_FLUSH(n)                               \
void n##Queue_flush(n##Queue *queue)                        \
{                                                           \
    if(!queue)                                              \
        return;                                             \
                                                            \
    n##List *sourceList, *next;                             \
                                                            \
    pthread_mutex_lock(&queue->mLock);                      \
    for(sourceList = queue->mFirst; sourceList != NULL;     \
            sourceList = next) {                            \
        next = sourceList->next;                            \
        n##_free(&(sourceList->source));                    \
        free(sourceList);                                   \
    }                                                       \
    queue->mLast = NULL;                                    \
    queue->mFirst = NULL;                                   \
    queue->mNumbers = 0;                                    \
    pthread_mutex_unlock(&queue->mLock);                    \
}                                                           \

#define DECLARE_FUNC_FREE(n)                                \
void n##Queue_free(n##Queue *queue)                         \
{                                                           \
    if(!queue)                                              \
        return;                                             \
                                                            \
    n##Queue_flush(queue);                                  \
    pthread_mutex_destroy(&queue->mLock);                   \
}                                                           \

#define DECLARE_FUNC_FREEP(n)                               \
void n##Queue_freep(n##Queue **queue)                       \
{                                                           \
    if(!queue || !*queue)                                   \
        return;                                             \
                                                            \
    n##Queue_free(*queue);                                  \
    free(*queue);                                           \
    *queue = NULL;                                          \
}                                                           \

#define DECLARE_FUNC_GET(n)                                 \
int n##Queue_get(n##Queue *queue, n *source)                \
{                                                           \
    n##List *sourceList;                                    \
    int ret = -1;                                           \
    if(!queue || !source)                                   \
        return ret;                                         \
                                                            \
    pthread_mutex_lock(&queue->mLock);                      \
    sourceList = queue->mFirst;                             \
    if(sourceList) {                                        \
        queue->mFirst = sourceList->next;                   \
        if (!queue->mFirst)                                 \
            queue->mLast = NULL;                            \
        queue->mNumbers--;                                  \
        *source = sourceList->source;                       \
        free(sourceList);                                   \
        ret = 1;                                            \
    } else {                                                \
        ret = -1;                                           \
    }                                                       \
    pthread_mutex_unlock(&queue->mLock);                    \
    return ret;                                             \
}                                                           \

#define DECLARE_FUNC_PUT(n)                                 \
int n##Queue_put(n##Queue *queue, n *source)                \
{                                                           \
    n##List *sourceList;                                    \
                                                            \
    if (!queue || !n##_isValid(source))                     \
        return -1;                                          \
                                                            \
    sourceList = (n##List *)calloc(1, sizeof(n##List));     \
    if (!sourceList)                                        \
        return -1;                                          \
    sourceList->source = *source;                           \
    sourceList->next = NULL;                                \
                                                            \
    pthread_mutex_lock(&queue->mLock);                      \
    if(!queue->mLast) {                                     \
        queue->mFirst = sourceList;                         \
    } else {                                                \
        queue->mLast->next = sourceList;                    \
    }                                                       \
    queue->mLast = sourceList;                              \
    queue->mNumbers++;                                      \
    pthread_mutex_unlock(&queue->mLock);                    \
    return 0;                                               \
}                                                           \

#define DECLARE_FUNC_CREATE(n)                              \
n##Queue *n##Queue_create()                                 \
{                                                           \
    n##Queue *queue =                                       \
        (n##Queue *)calloc(1, sizeof(n##Queue));            \
    if (!queue)                                             \
        return NULL;                                        \
                                                            \
    pthread_mutex_init(&queue->mLock, NULL);                \
    return queue;                                           \
}                                                           \

