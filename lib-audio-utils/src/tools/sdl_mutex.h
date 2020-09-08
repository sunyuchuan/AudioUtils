#ifndef SDL_MUTEX_H_
#define SDL_MUTEX_H_

typedef struct SdlMutexT SdlMutex;

#ifdef __cplusplus
extern "C" {
#endif

SdlMutex* sdl_mutex_create();
void sdl_mutex_free(SdlMutex** inst);
int sdl_mutex_lock(SdlMutex* inst);
int sdl_mutex_unlock(SdlMutex* inst);
int sdl_mutex_cond_signal(SdlMutex* inst);
int sdl_mutex_broadcast(SdlMutex* inst);
int sdl_mutex_wait_timeout(SdlMutex* inst, unsigned int ms);
int sdl_mutex_wait(SdlMutex* inst);

#ifdef __cplusplus
}
#endif
#endif  // SDL_MUTEX_H_
