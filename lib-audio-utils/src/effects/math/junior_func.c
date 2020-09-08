#include "junior_func.h"

float round_float(float in) {
    float a = in + 0.5f;
    int b = (int)in, c = (int)a;
    if (c != b) {
        return (float)c;
    } else {
        return (float)b;
    }
}