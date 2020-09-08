#ifndef _FREQUENCY_LIMITER_H_
#define _FREQUENCY_LIMITER_H_

const float a1[3] = {0.04589f, -0.0912487f, 0.04589f};
const float b1[2] = {-1.975961f, 0.9779f};
const float a2[2] = {0.0338585f, 0.0338585f};  // q15,q15
const float b2[1] = {-0.974798f};              // q15,q15

float sa1[2] = {0.0f, 0.0f};
float sb1[2] = {0.0f, 0.0f};
float sa2[1] = {0.0f};
float sb2[1] = {0.0f};

#endif