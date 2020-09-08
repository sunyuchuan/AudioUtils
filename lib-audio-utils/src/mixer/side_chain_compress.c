#include "side_chain_compress.h"
#include <math.h>

void side_chain_compress(short *voice, short *bgm, float *bgm_yl_prev,
        int buffer_size, int sample_rate, int nb_channels, float threshold,
        float ratio, float attack_ms, float release_ms, float makeup_gain) {
    if (!voice || !bgm) {
        return;
    }
    //used in control voltage calculation
    float xg, xl, yg, yl;
    //Previous sample used for gain smoothing
    float yl_prev = *bgm_yl_prev;
    //Output control voltage used for compression
    float gain = 1.0f;
    // Compression : calculates the control voltage
    float alphaAttack = exp(-1 / (0.001 * sample_rate * attack_ms));
    float alphaRelease = exp(-1 / (0.001 * sample_rate * release_ms));

    for (int i = 0; i < buffer_size;)
    {
	float voice_flp = voice[i] / (float)32767;
	// Level detection- estimate level using peak detector
	if (fabs(voice_flp) < 0.000001) xg = -120;
	else xg = 20 * log10(fabs(voice_flp));

	// Gain computer- static apply input/output curve
	if (xg >= threshold) yg = threshold + (xg - threshold) / ratio;
	else yg = xg;

	xl = xg - yg;
	// Ballistics- smoothing of the gain
	if (xl > yl_prev) yl = alphaAttack * yl_prev + (1 - alphaAttack) * xl;
	else yl = alphaRelease * yl_prev + (1 - alphaRelease) * xl;
	// find control
	gain = pow(10, ((MAKEUP_GAIN_MAX_DB * makeup_gain  - yl) / 20));
	yl_prev = yl;
	if (nb_channels == 2) {
	    bgm[i] = bgm[i] * gain;
	    if (i + 1 < buffer_size) bgm[i + 1] =bgm[i + 1] * gain;
	    i += 2;
	} else {
	    bgm[i] =bgm[i] * gain;
	    ++ i;
	}
    }
    *bgm_yl_prev = yl_prev;
}

