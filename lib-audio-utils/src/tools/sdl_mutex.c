#include "sdl_mutex.h"
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#define SDL_MUTEX_TIMEDOUT 1
#define EINTR 4       /* Interrupted system call */
#define ETIMEDOUT 110 /* Connection timed out */

struct SdlMutexT {
    pthread_mutex_t mutex_id;
    pthread_cond_t cond_id;
};

SdlMutex* sdl_mutex_create() {
    SdlMutex* self = (SdlMutex*)calloc(1, sizeof(SdlMutex));
    if (NULL == self) return NULL;

    if (pthread_mutex_init(&self->mutex_id, NULL) != 0) {
        free(self);
        return NULL;
    }

    if (pthread_cond_init(&self->cond_id, NULL) != 0) {
        pthread_mutex_destroy(&self->mutex_id);
        free(self);
        return NULL;
    }
    return self;
}

void sdl_mutex_free(SdlMutex** inst) {
    if (NULL == inst || NULL == *inst) return;
    SdlMutex* self = *inst;
    pthread_mutex_destroy(&self->mutex_id);
    pthread_cond_destroy(&self->cond_id);
    free(*inst);
    *inst = NULL;
}

int sdl_mutex_lock(SdlMutex* inst) {
    assert(NULL != inst);
    return pthread_mutex_lock(&inst->mutex_id);
}

int sdl_mutex_unlock(SdlMutex* inst) {
    assert(NULL != inst);
    return pthread_mutex_unlock(&inst->mutex_id);
}

int sdl_mutex_cond_signal(SdlMutex* inst) {
    assert(NULL != inst);
    return pthread_cond_signal(&inst->cond_id);
}

int sdl_mutex_broadcast(SdlMutex* inst) {
    assert(NULL != inst);
    return pthread_cond_broadcast(&inst->cond_id);
}

int sdl_mutex_wait_timeout(SdlMutex* inst, unsigned int ms) {
    assert(NULL != inst);
    int retval;
    struct timeval delta;
    struct timespec abstime;

    gettimeofday(&delta, NULL);

    abstime.tv_sec = delta.tv_sec + (ms / 1000);
    abstime.tv_nsec = (delta.tv_usec + (ms % 1000) * 1000) * 1000;
    if (abstime.tv_nsec > 1000000000) {
        abstime.tv_sec += 1;
        abstime.tv_nsec -= 1000000000;
    }

    while (1) {
        retval =
            pthread_cond_timedwait(&inst->cond_id, &inst->mutex_id, &abstime);
        if (retval == 0)
            return 0;
        else if (retval == EINTR)
            continue;
        else if (retval == ETIMEDOUT)
            return SDL_MUTEX_TIMEDOUT;
        else
            break;
    }

    return -1;
}
int sdl_mutex_wait(SdlMutex* inst) {
    assert(NULL != inst);
    return pthread_cond_wait(&inst->cond_id, &inst->mutex_id);
}