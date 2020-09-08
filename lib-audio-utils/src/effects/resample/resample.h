#ifndef _MINIONS_RESAMPLE_H_
#define _MINIONS_RESAMPLE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <libswresample/swresample.h>

int resampler_init(const int src_channels, const int dst_channels,
                   const int src_sample_rate, const int dst_sample_rate,
                   const enum AVSampleFormat src_sample_fmt,
                   const enum AVSampleFormat dst_sample_fmt,
                   SwrContext** swr_ctx);
int allocate_sample_buffer(uint8_t*** buffer, const int nb_channels,
                           const int nb_samples,
                           const enum AVSampleFormat sample_fmt);
int resample_audio(SwrContext* swr_ctx, uint8_t** src_samples,
                   const int src_nb_samples, uint8_t*** dst_samples,
                   int* max_dst_nb_samples, const int dst_channels);
#ifdef __cplusplus
}
#endif
#endif  // RESAMPLE_H_
