#ifndef _SIDE_CHAIN_COMPRESS_H_
#define _SIDE_CHAIN_COMPRESS_H_

#define SIDE_CHAIN_THRESHOLD (-50)
#define SIDE_CHAIN_RATIO 10
#define SIDE_CHAIN_ATTACK_MS 100
#define SIDE_CHAIN_RELEASE_MS 300
#define MAKEUP_GAIN_MAX_DB 5

void side_chain_compress(short *voice, short *bgm, float *bgm_yl_prev,
        int buffer_size, int sample_rate, int nb_channels, float threshold,
        float ratio, float attack_ms, float release_ms, float makeup_gain);
#endif
