#ifndef FAST_MATH_OPS_H_
#define FAST_MATH_OPS_H_

#define MAGIC 0x5f3759df
#define THREEHALFS 1.5f

static inline float RecipSqrt(float number) {
    int i, j;
    float x2, y, y2, tmp;

    x2 = number * 0.5F;
    y = number;
    int *int_tmp = (int*)&y;
    i = *int_tmp;
    j = MAGIC - (i >> 1);
    float *float_tmp = (float*)&j;
    y = *float_tmp;
    y2 = y * y;
    tmp = THREEHALFS - x2 * y2;

    return y * tmp;
}

static inline float Recip(float number) {
    int i, j;
    float x2, y, y2, tmp;

    x2 = number * 0.5F;
    y = number;
    int *int_tmp = (int*)&y;
    i = *int_tmp;
    j = MAGIC - (i >> 1);
    float *float_tmp = (float*)&j;
    y = *float_tmp;
    y2 = y * y;
    tmp = THREEHALFS - x2 * y2;
    y *= tmp;
    return y * y;
}

static inline float Log10(const float val, register float* const pTable,
                          register const unsigned precision) {
    /* get access to float bits */
    register const int* const pVal = (const int*)(&val);

    /* extract exponent and mantissa (quantized) */
    register const int exp = ((*pVal >> 23) & 255) - 127;
    register const int man = (*pVal & 0x7FFFFF) >> (23 - precision);

    /* exponent plus lookup refinement */
    return ((float)(exp) + pTable[man]) * 0.301029995663981f;
}

#endif  // FAST_MATH_OPS_H
