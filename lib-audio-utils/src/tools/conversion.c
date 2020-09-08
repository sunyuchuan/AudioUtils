//
// Created by layne on 19-4-28.
//

#include "conversion.h"
#include <stdint.h>

#define FFMAX(a, b) ((a) > (b) ? (a) : (b))
#define FFMIN(a, b) ((a) > (b) ? (b) : (a))

void S16ToFloat(const int16_t *src, float *dst, int nb_samples) {
    const int16_t *p = src;
    float *q = dst;
    int n = nb_samples;

    while (n >= 4) {
        q[0] = (float)(p[0]) / 32768.0f;
        q[1] = (float)(p[1]) / 32768.0f;
        q[2] = (float)(p[2]) / 32768.0f;
        q[3] = (float)(p[3]) / 32768.0f;
        q += 4;
        p += 4;
        n -= 4;
    }
    while (n > 0) {
        q[0] = (float)(p[0]) / 32768.0f;
        q++;
        p++;
        n--;
    }
}

void FloatToS16(float *src, int16_t *dst, int nb_samples) {
    const float *p = src;
    int16_t *q = dst;
    int n = nb_samples;

    while (n >= 4) {
        q[0] = (int16_t)FFMIN(32767.0f, FFMAX(-32768.0f, p[0] * 32767));
        q[1] = (int16_t)FFMIN(32767.0f, FFMAX(-32768.0f, p[1] * 32767));
        q[2] = (int16_t)FFMIN(32767.0f, FFMAX(-32768.0f, p[2] * 32767));
        q[3] = (int16_t)FFMIN(32767.0f, FFMAX(-32768.0f, p[3] * 32767));
        q += 4;
        p += 4;
        n -= 4;
    }
    while (n > 0) {
        q[0] = (int16_t)FFMIN(32767.0f, FFMAX(-32768.0f, p[0] * 32767));
        q++;
        p++;
        n--;
    }
}