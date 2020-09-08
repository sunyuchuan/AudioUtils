#include "resample.h"
#include <libavutil/channel_layout.h>
#include "log.h"

int resampler_init(const int src_channels, const int dst_channels,
                   const int src_sample_rate, const int dst_sample_rate,
                   const enum AVSampleFormat src_sample_fmt,
                   const enum AVSampleFormat dst_sample_fmt,
                   SwrContext** swr_ctx) {
    int ret = 0;

    if (*swr_ctx) swr_free(swr_ctx);

    if (src_channels != dst_channels || src_sample_rate != dst_sample_rate ||
        src_sample_rate != dst_sample_fmt) {
        *swr_ctx = swr_alloc_set_opts(
            NULL, av_get_default_channel_layout(dst_channels), dst_sample_fmt,
            dst_sample_rate, av_get_default_channel_layout(src_channels),
            src_sample_fmt, src_sample_rate, 0, NULL);
        if (NULL == *swr_ctx) {
            LogError("Could not allocate resample context\n");
            ret = AVERROR(ENOMEM);
        }
        // Open the resampler with the specified parameters.
        ret = swr_init(*swr_ctx);
        if (ret < 0) {
            LogError("swr_init error(%s) error code = %d\n", av_err2str(ret),
                     ret);
            goto end;
        }
    }

end:
    if (ret < 0 && *swr_ctx) swr_free(swr_ctx);
    return ret;
}

int allocate_sample_buffer(uint8_t*** buffer, const int nb_channels,
                           const int nb_samples,
                           const enum AVSampleFormat sample_fmt) {
    if (*buffer) {
        av_freep(&((*buffer)[0]));
        av_freep(buffer);
    }
    int ret = av_samples_alloc_array_and_samples(buffer, NULL, nb_channels,
                                                 nb_samples, sample_fmt, 0);
    if (ret < 0) {
        LogError("Could not allocate source samples(%s) error code = %d\n",
                 av_err2str(ret), ret);
        goto end;
    }
end:
    return ret;
}

int resample_audio(SwrContext* swr_ctx, uint8_t** src_samples,
                   const int src_nb_samples, uint8_t*** dst_samples,
                   int* max_dst_nb_samples, const int dst_channels) {
    int ret = 0;
    int dst_nb_samples = swr_get_out_samples(swr_ctx, src_nb_samples);
    if (dst_nb_samples > *max_dst_nb_samples) {
        *max_dst_nb_samples = dst_nb_samples;
        av_freep(&(*dst_samples)[0]);
        ret = av_samples_alloc(*dst_samples, NULL, dst_channels,
                               *max_dst_nb_samples, AV_SAMPLE_FMT_S16, 1);
        if (ret < 0) {
            LogError(
                "resample.c:%d %s av_samples_alloc error, error code = %d.\n",
                __LINE__, __func__, ret);
            goto end;
        }
    }
    // Convert to destination format
    ret = dst_nb_samples =
        swr_convert(swr_ctx, *dst_samples, dst_nb_samples,
                    (const uint8_t**)(src_samples), src_nb_samples);
    if (ret < 0) {
        LogError("resample.cpp:%d %s swr_convert, error code = %d.\n", __LINE__,
                 __func__, ret);
        goto end;
    }
end:
    return ret;
}
