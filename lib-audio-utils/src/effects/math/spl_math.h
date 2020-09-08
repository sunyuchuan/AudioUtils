#ifndef SPL_MATH_H
#define SPL_MATH_H

#define SPL_MUL_16_16(a, b) ((int)(((short)(a)) * ((short)(b))))
#define SPL_MUL_16_32_RSFT16(a, b) \
    (SPL_MUL_16_16(a, b >> 16) +   \
     ((SPL_MUL_16_16(a, (b & 0xffff) >> 1) + 0x4000) >> 15))

#define SPL_MUL_16_32_RSFT16(a, b) \
    (SPL_MUL_16_16(a, b >> 16) +   \
     ((SPL_MUL_16_16(a, (b & 0xffff) >> 1) + 0x4000) >> 15))

#define FFMAX(a, b) ((a) > (b) ? (a) : (b))
#define FFMIN(a, b) ((a) > (b) ? (b) : (a))

#endif