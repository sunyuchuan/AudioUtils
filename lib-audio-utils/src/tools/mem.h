//
// Created by layne on 19-4-27.
//

#ifndef AUDIO_EFFECT_MEM_H
#define AUDIO_EFFECT_MEM_H


#include <stddef.h>

void av_max_alloc(size_t max);
void *av_malloc(size_t size);
void *av_realloc(void *ptr, size_t size);
void *av_realloc_f(void *ptr, size_t nelem, size_t elsize);
int av_reallocp(void *ptr, size_t size);
void *av_realloc_array(void *ptr, size_t nmemb, size_t size);
int av_reallocp_array(void *ptr, size_t nmemb, size_t size);
void av_free(void *ptr);
void av_freep(void *arg);
void *av_mallocz(size_t size);
void *av_calloc(size_t nmemb, size_t size);
char *av_strdup(const char *s);
char *av_strndup(const char *s, size_t len);
void *av_memdup(const void *p, size_t size);

#endif  // AUDIO_EFFECT_MEM_H
