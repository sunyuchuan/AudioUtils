//
// Created by layne on 19-4-27.
//
#include "mem.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "error_def.h"

#define ALIGN (HAVE_AVX ? 32 : 16)

static size_t max_alloc_size = INT_MAX;

static inline int av_size_mult(size_t a, size_t b, size_t *r) {
    size_t t = a * b;
    if ((a | b) >= ((size_t)1 << (sizeof(size_t) * 4)) && a && t / a != b)
        return AEERROR_INVAL;
    *r = t;
    return 0;
}

void av_max_alloc(size_t max) { max_alloc_size = max; }

void *av_malloc(size_t size) {
    if (size > (max_alloc_size - 32)) return NULL;

    void *ptr = NULL;
    ptr = malloc(size);

    if (!ptr && !size) {
        size = 1;
        ptr = av_malloc(1);
    }
    return ptr;
}

void *av_realloc(void *ptr, size_t size) {
    if (size > (max_alloc_size - 32)) return NULL;
    return realloc(ptr, size + !size);
}

void *av_realloc_f(void *ptr, size_t nelem, size_t elsize) {
    size_t size;
    void *r;

    if (av_size_mult(elsize, nelem, &size)) {
        av_free(ptr);
        return NULL;
    }
    r = av_realloc(ptr, size);
    if (!r) av_free(ptr);
    return r;
}

int av_reallocp(void *ptr, size_t size) {
    void *val;

    if (!size) {
        av_freep(ptr);
        return 0;
    }

    memcpy(&val, ptr, sizeof(val));
    val = av_realloc(val, size);

    if (!val) {
        av_freep(ptr);
        return AEERROR_NOMEM;
    }

    memcpy(ptr, &val, sizeof(val));
    return 0;
}

void *av_realloc_array(void *ptr, size_t nmemb, size_t size) {
    if (!size || nmemb >= INT_MAX / size) return NULL;
    return av_realloc(ptr, nmemb * size);
}

int av_reallocp_array(void *ptr, size_t nmemb, size_t size) {
    void *val;

    memcpy(&val, ptr, sizeof(val));
    val = av_realloc_f(val, nmemb, size);
    memcpy(ptr, &val, sizeof(val));
    if (!val && nmemb && size) return AEERROR_NOMEM;

    return 0;
}

void av_free(void *ptr) { free(ptr); }

void av_freep(void *arg) {
    void *val;

    memcpy(&val, arg, sizeof(val));
    memcpy(arg, &(void *){NULL}, sizeof(val));
    av_free(val);
}

void *av_mallocz(size_t size) {
    void *ptr = av_malloc(size);
    if (ptr) memset(ptr, 0, size);
    return ptr;
}

void *av_calloc(size_t nmemb, size_t size) {
    if (size <= 0 || nmemb >= INT_MAX / size) return NULL;
    return av_mallocz(nmemb * size);
}

char *av_strdup(const char *s) {
    char *ptr = NULL;
    if (s) {
        size_t len = strlen(s) + 1;
        ptr = av_realloc(NULL, len);
        if (ptr) memcpy(ptr, s, len);
    }
    return ptr;
}

char *av_strndup(const char *s, size_t len) {
    char *ret = NULL, *end;

    if (!s) return NULL;

    end = memchr(s, 0, len);
    if (end) len = end - s;

    ret = av_realloc(NULL, len + 1);
    if (!ret) return NULL;

    memcpy(ret, s, len);
    ret[len] = 0;
    return ret;
}

void *av_memdup(const void *p, size_t size) {
    void *ptr = NULL;
    if (p) {
        ptr = av_malloc(size);
        if (ptr) memcpy(ptr, p, size);
    }
    return ptr;
}
