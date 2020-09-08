//
// Created by layne on 19-4-28.
//

#ifndef AUDIO_EFFECT_CONVERSION_H
#define AUDIO_EFFECT_CONVERSION_H

void S16ToFloat(const short *src, float *dst, int nb_samples);
void FloatToS16(float *src, short *dst, int nb_samples);

#endif  // AUDIO_EFFECT_CONVERSION_H
