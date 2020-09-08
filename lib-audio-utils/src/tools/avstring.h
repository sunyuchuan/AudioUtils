//
// Created by layne on 19-4-27.
//

#ifndef AUDIO_EFFECT_AVSTRING_H
#define AUDIO_EFFECT_AVSTRING_H

#include <stddef.h>

size_t av_strlcat(char *dst, const char *src, size_t size);
size_t av_strlcpy(char *dst, const char *src, size_t size);
char *av_get_token(const char **buf, const char *term);
char *av_strnstr(const char *haystack, const char *needle, size_t hay_length);
size_t av_strlcatf(char *dst, size_t size, const char *fmt, ...);

#endif  // AUDIO_EFFECT_AVSTRING_H
