#if defined(__ANDROID__) || defined (__linux__)

#ifndef __AUDIO_ENCODER_SW_H__
#define __AUDIO_ENCODER_SW_H__

#include "codec/audio_encoder.h"

Encoder *ff_encoder_sw_create();

#endif
#endif
