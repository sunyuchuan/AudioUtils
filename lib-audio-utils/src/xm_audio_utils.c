#include "xm_audio_utils.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "log.h"
#include "error_def.h"
#include "tools/util.h"
#include "codec/audio_decoder_factory.h"
#include "mixer/fade_in_out.h"
#include "mixer/xm_audio_mixer.h"

typedef struct PcmResampler {
    int src_sample_rate;
    int src_nb_channels;
    double dst_sample_rate;
    int dst_nb_channels;
    long cur_samples_pos;
    short *buffer;
    int buffer_nb_samples;
    int sample_interval;
    IAudioDecoder *decoder;
} PcmResampler;

typedef struct Fade {
    int bgm_start_time_ms;
    int bgm_end_time_ms;
    int pcm_sample_rate;
    int pcm_nb_channels;
    FadeInOut fade_io;
} Fade;

struct XmAudioUtils {
    volatile int ref_count;
    IAudioDecoder *decoder;
    XmMixerContext *mixer_ctx;
    Fade *fade;
    PcmResampler *pcm_resampler;
    pthread_mutex_t mutex;
};

static void pcm_resampler_release(PcmResampler **swr)
{
    if (!swr || !*swr)
        return;

    if ((*swr)->decoder) {
        IAudioDecoder_freep(&(*swr)->decoder);
    }
    if ((*swr)->buffer) {
        free((*swr)->buffer);
        (*swr)->buffer = NULL;
    }
    free(*swr);
    *swr = NULL;
}

void xmau_inc_ref(XmAudioUtils *self)
{
    assert(self);
    __sync_fetch_and_add(&self->ref_count, 1);
}

void xmau_dec_ref(XmAudioUtils *self)
{
    if (!self)
        return;

    int ref_count = __sync_sub_and_fetch(&self->ref_count, 1);
    if (ref_count == 0) {
        LogInfo("%s xmau_dec_ref(): ref=0\n", __func__);
        xm_audio_utils_freep(&self);
    }
}

void xmau_dec_ref_p(XmAudioUtils **self)
{
    if (!self || !*self)
        return;

    xmau_dec_ref(*self);
    *self = NULL;
}

void xm_audio_utils_free(XmAudioUtils *self) {
    LogInfo("%s\n", __func__);
    if (NULL == self)
        return;

    if (self->pcm_resampler) {
        pcm_resampler_release(&self->pcm_resampler);
    }
    if (self->fade) {
        free(self->fade);
        self->fade = NULL;
    }
    if (self->decoder) {
        IAudioDecoder_freep(&self->decoder);
    }
    if (self->mixer_ctx) {
        xm_audio_mixer_stop(self->mixer_ctx);
        xm_audio_mixer_freep(&(self->mixer_ctx));
    }
}

void xm_audio_utils_freep(XmAudioUtils **au) {
    LogInfo("%s\n", __func__);
    if (NULL == au || NULL == *au)
        return;
    XmAudioUtils *self = *au;

    xm_audio_utils_free(self);
    pthread_mutex_destroy(&self->mutex);
    free(*au);
    *au = NULL;
}

int xm_audio_utils_mixer_get_frame(XmAudioUtils *self,
    short *buffer, int buffer_size_in_short) {
    if (!self || !buffer || buffer_size_in_short <= 0) {
        return -1;
    }

    return xm_audio_mixer_get_frame(self->mixer_ctx,
        buffer, buffer_size_in_short);
}

int xm_audio_utils_mixer_seekTo(XmAudioUtils *self,
    int seek_time_ms) {
    LogInfo("%s seek_time_ms %d\n", __func__, seek_time_ms);
    if (!self) {
        return -1;
    }

    return xm_audio_mixer_seekTo(self->mixer_ctx, seek_time_ms);
}

int xm_audio_utils_mixer_init(XmAudioUtils *self,
        const char *in_config_path) {
    LogInfo("%s\n", __func__);
    int ret = -1;
    if (!self || !in_config_path) {
        return -1;
    }

    xm_audio_mixer_stop(self->mixer_ctx);
    xm_audio_mixer_freep(&(self->mixer_ctx));

    self->mixer_ctx = xm_audio_mixer_create();
    if (!self->mixer_ctx) {
        LogError("%s xm_audio_mixer_create failed\n", __func__);
        ret = -1;
        goto end;
    }

    ret = xm_audio_mixer_init(self->mixer_ctx, in_config_path);
    if (ret < 0) {
        LogError("%s xm_audio_mixer_init failed\n", __func__);
        goto end;
    }

end:
    return ret;
}

int xm_audio_utils_fade(XmAudioUtils *self, short *buffer,
        int buffer_size, int buffer_start_time) {
    if (!self || !buffer || !self->fade || buffer_size <= 0)
        return -1;
    Fade *fade = self->fade;
    int buffer_duration_ms = 1000 * ((float)buffer_size /
        fade->pcm_nb_channels / fade->pcm_sample_rate);

    check_fade_in_out(&fade->fade_io, buffer_start_time, buffer_duration_ms,
        fade->pcm_sample_rate, fade->bgm_start_time_ms, fade->bgm_end_time_ms);
    scale_with_ramp(&fade->fade_io, buffer,
        buffer_size / fade->pcm_nb_channels, fade->pcm_nb_channels);
    return 0;
}

int xm_audio_utils_fade_init(XmAudioUtils *self,
        int pcm_sample_rate, int pcm_nb_channels,
        int bgm_start_time_ms, int bgm_end_time_ms,
        int fade_in_time_ms, int fade_out_time_ms) {
    if (!self)
        return -1;
    LogInfo("%s\n", __func__);

    if (self->fade) {
        free(self->fade);
        self->fade = NULL;
    }
    self->fade = (Fade *)calloc(1, sizeof(Fade));
    if (NULL == self->fade) {
        LogError("%s calloc Fade failed.\n", __func__);
        return -1;
    }

    Fade *fade = self->fade;
    fade->bgm_start_time_ms = bgm_start_time_ms;
    fade->bgm_end_time_ms = bgm_end_time_ms;
    fade->pcm_sample_rate = pcm_sample_rate;
    fade->pcm_nb_channels = pcm_nb_channels;
    fade->fade_io.fade_in_time_ms = fade_in_time_ms;
    fade->fade_io.fade_out_time_ms = fade_out_time_ms;
    fade->fade_io.fade_in_nb_samples = fade_in_time_ms * pcm_sample_rate / 1000;
    fade->fade_io.fade_out_nb_samples = fade_out_time_ms * pcm_sample_rate / 1000;
    fade->fade_io.fade_in_samples_count = 0;
    fade->fade_io.fade_out_samples_count = 0;
    fade->fade_io.fade_out_start_index = 0;
    return 0;
}

int xm_audio_utils_get_decoded_frame(XmAudioUtils *self,
    short *buffer, int buffer_size_in_short, bool loop) {
    int ret = -1;
    if(NULL == self || NULL == buffer
        || buffer_size_in_short <= 0) {
        return ret;
    }

    ret = IAudioDecoder_get_pcm_frame(self->decoder, buffer, buffer_size_in_short, loop);
    if (PCM_FILE_EOF == ret) ret = 0;
    return ret;
}

void xm_audio_utils_decoder_seekTo(XmAudioUtils *self,
        int seek_time_ms) {
    LogInfo("%s seek_time_ms %d\n", __func__, seek_time_ms);
    if(NULL == self) {
        return;
    }

    IAudioDecoder_seekTo(self->decoder, seek_time_ms);
}

int xm_audio_utils_decoder_create(XmAudioUtils *self,
    const char *in_audio_path, int crop_start_time_in_ms, int crop_end_time_in_ms,
    int out_sample_rate, int out_channels, int volume_fix) {
    LogInfo("%s\n", __func__);
    if (NULL == self || NULL == in_audio_path) {
        return -1;
    }

    float volume_flp = volume_fix / (float)100;
    IAudioDecoder_freep(&self->decoder);
    self->decoder = audio_decoder_create(in_audio_path, 0, 0,
        out_sample_rate, out_channels, volume_flp, DECODER_FFMPEG);
    if (self->decoder == NULL) {
        LogError("audio_decoder_create failed\n");
        return -1;
    }

    if (IAudioDecoder_set_crop_pos(self->decoder,
        crop_start_time_in_ms, crop_end_time_in_ms) < 0) {
        LogError("%s IAudioDecoder_set_crop_pos failed.\n", __func__);
        IAudioDecoder_freep(&self->decoder);
        return -1;
    }

    return 0;
}

int xm_audio_utils_pcm_resampler_resample(
    XmAudioUtils *self, short *buffer, int buffer_size_in_short) {
    if (!self || !buffer || !self->pcm_resampler) return -1;

    PcmResampler *swr = self->pcm_resampler;
    if (!swr->decoder) return -1;

    int cur_nb_samples = 0;
    int dst_nb_samples = buffer_size_in_short / swr->dst_nb_channels;
    while (swr->sample_interval * (dst_nb_samples - cur_nb_samples)
            >= swr->buffer_nb_samples) {
        int read_len = IAudioDecoder_get_pcm_frame(swr->decoder, swr->buffer,
            swr->buffer_nb_samples * swr->src_nb_channels, false);
        if (read_len <= 0) {
            break;
        }

        for (int i = 0; i < read_len; i += swr->src_nb_channels) {
            if (((swr->cur_samples_pos + (i/swr->src_nb_channels))
                % swr->sample_interval) == 0) {
                if (swr->dst_nb_channels == 1) {
                    if (swr->src_nb_channels == 1) {
                        buffer[cur_nb_samples] = swr->buffer[i];
                    } else {
                        buffer[cur_nb_samples] = (short) ((swr->buffer[i] + swr->buffer[i + 1]) >> 1);
                    }
                } else {
                    if (swr->src_nb_channels == 1) {
                        buffer[2 * cur_nb_samples] = swr->buffer[i];
                        buffer[2 * cur_nb_samples + 1] = swr->buffer[i];
                    } else {
                        buffer[2 * cur_nb_samples] = swr->buffer[i];
                        buffer[2 * cur_nb_samples + 1] = swr->buffer[i + 1];
                    }
                }
                cur_nb_samples ++;
            }
        }
        swr->cur_samples_pos += read_len / swr->src_nb_channels;
    }

    return cur_nb_samples * swr->dst_nb_channels;
}

bool xm_audio_utils_pcm_resampler_init(
    XmAudioUtils *self, const char *in_audio_path, bool is_pcm, int src_sample_rate,
    int src_nb_channels, double dst_sample_rate, int dst_nb_channels) {
    LogInfo("%s\n", __func__);
    if (!self || !in_audio_path
            || (is_pcm && src_nb_channels != 1 && src_nb_channels != 2)) return false;
    if (dst_sample_rate <= 0
            || (dst_nb_channels != 1 && dst_nb_channels != 2)) return false;

    pcm_resampler_release(&self->pcm_resampler);
    self->pcm_resampler = (PcmResampler *)calloc(1, sizeof(PcmResampler));
    if (!self->pcm_resampler) {
        LogError("%s calloc PcmResampler failed.\n", __func__);
        goto fail;
    }

    PcmResampler *swr = self->pcm_resampler;
    int out_sample_rate = 0;
    int out_channels = 1;
    enum DecoderType type;
    if(is_pcm) {
        type = DECODER_PCM;
        out_sample_rate = src_sample_rate;
    } else {
        type = DECODER_FFMPEG;
        out_sample_rate = 44100;
    }

    swr->decoder = audio_decoder_create(in_audio_path, src_sample_rate,
        src_nb_channels, out_sample_rate, out_channels, 1.0f, type);
    if (!swr->decoder) {
        LogError("%s audio_decoder_create failed.\n", __func__);
        goto fail;
    }

    swr->src_sample_rate = out_sample_rate;
    swr->src_nb_channels = out_channels;
    swr->dst_sample_rate = dst_sample_rate;
    swr->dst_nb_channels = dst_nb_channels;
    swr->cur_samples_pos = 0;
    swr->sample_interval = swr->src_sample_rate / swr->dst_sample_rate;

    swr->buffer_nb_samples = 512;
    swr->buffer = (short *)calloc(sizeof(short), swr->buffer_nb_samples * swr->src_nb_channels);
    if (!swr->buffer) {
        LogError("%s calloc swr->buffer failed.\n", __func__);
        goto fail;
    }

    return true;
fail:
    pcm_resampler_release(&self->pcm_resampler);
    return false;
}

XmAudioUtils *xm_audio_utils_create() {
    XmAudioUtils *self = (XmAudioUtils *)calloc(1, sizeof(XmAudioUtils));
    if (NULL == self) {
        LogError("%s alloc XmAudioUtils failed.\n", __func__);
        return NULL;
    }

    pthread_mutex_init(&self->mutex, NULL);
    xmau_inc_ref(self);
    return self;
}

