#include "low_pass.h"
#include "frequency_limiter.h"


void LowPassIIR(float *in, short in_len, float *out, short *out_len) {
    short i;
    float tmpno1, tmpno2, tmpno3, tmpno4;
    for (i = 0; i < in_len; i++) {
        tmpno1 = in[i] * a1[0];
        tmpno2 = sa1[0] * a1[1];
        tmpno3 = sa1[1] * a1[2] + tmpno1;
        sa1[1] = sa1[0];
        tmpno4 = tmpno2 - b1[0] * sb1[0];
        sa1[0] = in[i];
        tmpno1 = tmpno3 - b1[1] * sb1[1];
        tmpno2 = tmpno4 + tmpno1;
        sb1[1] = sb1[0];
        tmpno3 = tmpno2 * a2[0];
        tmpno4 = sa2[0] * a2[1] + tmpno3;
        sb1[0] = tmpno2;
        sa2[0] = tmpno2;
        tmpno1 = tmpno4 - sb2[0] * b2[0];
        out[i] = tmpno1;
        sb2[0] = tmpno1;
    }
    *out_len = in_len;
}

float IIR_DR2(float x, float *plast, const float(*A)[3], const float(*B)[3])
{
	float tmp, last;

	tmp = x * B[0][0];

	last = tmp - (A[1][1] * plast[0] + A[1][2] * plast[1]);
	tmp = last + (B[1][1] * plast[0] + B[1][2] * plast[1]);

	plast[1] = plast[0];
	plast[0] = last;

	return tmp;
}

