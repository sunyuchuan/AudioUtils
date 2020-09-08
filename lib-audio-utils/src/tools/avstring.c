//
// Created by layne on 19-4-27.
//

#include "avstring.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#if defined(__ANDROID__) || defined (__linux__)
#include "libavutil/mem.h"
#else
#include "mem.h"
#endif

size_t av_strlcat(char *dst, const char *src, size_t size) {
    size_t len = strlen(dst);
    if (size <= len + 1) return len + strlen(src);
    return len + av_strlcpy(dst + len, src, size - len);
}

size_t av_strlcpy(char *dst, const char *src, size_t size) {
    size_t len = 0;
    while (++len < size && *src) *dst++ = *src++;
    if (len <= size) *dst = 0;
    return len + strlen(src) - 1;
}

#define WHITESPACES " \n\t\r"

char *av_get_token(const char **buf, const char *term) {
    char *out = av_malloc(strlen(*buf) + 1);
    char *ret = out, *end = out;
    const char *p = *buf;
    if (!out) return NULL;
    p += strspn(p, WHITESPACES);

    while (*p && !strspn(p, term)) {
        char c = *p++;
        if (c == '\\' && *p) {
            *out++ = *p++;
            end = out;
        } else if (c == '\'') {
            while (*p && *p != '\'') *out++ = *p++;
            if (*p) {
                p++;
                end = out;
            }
        } else {
            *out++ = c;
        }
    }

    do
        *out-- = 0;
    while (out >= end && strspn(out, WHITESPACES));

    *buf = p;

    return ret;
}

char *av_strnstr(const char *haystack, const char *needle, size_t hay_length) {
    size_t needle_len = strlen(needle);
    if (!needle_len) return (char *)haystack;
    while (hay_length >= needle_len) {
        hay_length--;
        if (!memcmp(haystack, needle, needle_len)) return (char *)haystack;
        haystack++;
    }
    return NULL;
}

size_t av_strlcatf(char *dst, size_t size, const char *fmt, ...) {
    size_t len = strlen(dst);
    va_list vl;

    va_start(vl, fmt);
    len += vsnprintf(dst + len, size > len ? size - len : 0, fmt, vl);
    va_end(vl);

    return len;
}