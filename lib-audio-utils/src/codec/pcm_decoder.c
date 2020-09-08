#include "pcm_decoder.h"
#include "log.h"
#include "error_def.h"
#include "tools/util.h"
#include "tools/fifo.h"
#include "ffmpeg_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct IAudioDecoder_Opaque {
    // seek parameters
    int seek_pos_ms;
    int64_t seek_pos_bytes;
    bool decode_completed;

    // play-out volume.
    short volume_fix;
    float volume_flp;

    // Input parameters
    int src_sample_rate_in_Hz;
    int src_nb_channels;
    int bits_per_sample;
    int64_t pcm_start_pos;
    int duration_ms;
    int64_t duration_bytes;

    // Output parameters
    int crop_start_time_in_ms;
    int crop_end_time_in_ms;
    int64_t crop_start_pos;
    int64_t crop_end_pos;
    int dst_sample_rate_in_Hz;
    int dst_nb_channels;

    fifo *pcm_fifo;
    int max_src_buffer_size;
    short *src_buffer;
    int max_dst_buffer_size;
    short *dst_buffer;

    char* file_addr;
    int64_t file_size;
    int64_t cur_pos;
    FILE *reader;
} IAudioDecoder_Opaque;

static void PcmDecoder_free(IAudioDecoder_Opaque *decoder);
static int PcmDecoder_set_crop_pos(IAudioDecoder_Opaque *decoder,
    int crop_start_time_ms, int crop_end_time_ms);

inline static int64_t align(int64_t x, int align) {
    return ((( x ) + (align) - 1) / (align) * (align));
}

static int64_t calculate_location_bytes(int time_in_ms,
    int bits_per_sample, int src_sample_rate_in_Hz, int src_nb_channels) {
    return align((bits_per_sample / 8) * (time_in_ms / (float) 1000) *
        src_nb_channels * (int64_t)src_sample_rate_in_Hz,
        (src_nb_channels * bits_per_sample / 8));
}

static int write_fifo(IAudioDecoder_Opaque *decoder) {
    int ret = -1;
    if (!decoder || !decoder->reader)
        return ret;

    if (decoder->cur_pos + decoder->seek_pos_bytes >= decoder->duration_bytes) {
        ret = PCM_FILE_EOF;
        goto end;
    }

    if (feof(decoder->reader) || ferror(decoder->reader)) {
        ret = PCM_FILE_EOF;
        goto end;
    }

    int read_len = fread(decoder->src_buffer, sizeof(*(decoder->src_buffer)),
        decoder->max_src_buffer_size, decoder->reader);
    if (read_len <= 0) {
        ret = PCM_FILE_EOF;
        goto end;
    }
    decoder->cur_pos += read_len * sizeof(*(decoder->src_buffer));

    set_gain(decoder->src_buffer, read_len, decoder->volume_fix);

    if (decoder->src_nb_channels != decoder->dst_nb_channels) {
        int write_size = 0;
        int nb_samples = 0;
        if (decoder->src_nb_channels == 1) {
            nb_samples = read_len;
            MonoToStereoS16(decoder->dst_buffer, decoder->src_buffer, nb_samples);
            write_size = read_len << 1;
        } else if (decoder->src_nb_channels == 2) {
            nb_samples = read_len >> 1;
            StereoToMonoS16(decoder->dst_buffer, decoder->src_buffer, nb_samples);
            write_size = read_len >> 1;
        }
        ret = fifo_write(decoder->pcm_fifo, decoder->dst_buffer, write_size);
        if (ret < 0) goto end;
        ret = write_size;
    } else {
        ret = fifo_write(decoder->pcm_fifo, decoder->src_buffer, read_len);
        if (ret < 0) goto end;
        ret = read_len;
    }

end:
    return ret;
}

static void get_duration_l(IAudioDecoder_Opaque *decoder) {
    if (!decoder || !decoder->reader)
        return;

    fseek(decoder->reader, 0, SEEK_END);
    decoder->file_size = ftell(decoder->reader) - decoder->pcm_start_pos;
    fseek(decoder->reader, decoder->pcm_start_pos, SEEK_SET);
    decoder->duration_ms = calculation_duration_ms(decoder->file_size,
        decoder->bits_per_sample/8, decoder->src_nb_channels,
        decoder->src_sample_rate_in_Hz);
    decoder->duration_bytes = decoder->file_size;
}

static void init_decoder_params(IAudioDecoder_Opaque *decoder,
    int src_sample_rate, int src_nb_channels,
    int dst_sample_rate, int dst_nb_channels, float volume_flp) {
    if (NULL == decoder)
        return;

    PcmDecoder_free(decoder);
    decoder->cur_pos = 0;
    decoder->seek_pos_ms = 0;
    decoder->seek_pos_bytes = 0;
    decoder->crop_start_time_in_ms = 0;
    decoder->crop_end_time_in_ms = 0;
    decoder->crop_start_pos = 0;
    decoder->crop_end_pos = 0;
    decoder->src_sample_rate_in_Hz = src_sample_rate;
    decoder->src_nb_channels = src_nb_channels;
    decoder->bits_per_sample = BITS_PER_SAMPLE_16;
    decoder->pcm_start_pos = 0x0;
    decoder->dst_sample_rate_in_Hz = dst_sample_rate;
    decoder->dst_nb_channels = dst_nb_channels;
    decoder->volume_flp = volume_flp;
    decoder->volume_fix = (short)(32767 * volume_flp);
    decoder->decode_completed = false;
}

static void init_timings_params(IAudioDecoder_Opaque *decoder,
    int crop_start_time_ms, int crop_end_time_ms) {
    if (!decoder) return;

    decoder->duration_ms =  calculation_duration_ms(decoder->file_size,
        decoder->bits_per_sample/8, decoder->src_nb_channels,
        decoder->src_sample_rate_in_Hz);

    decoder->crop_start_time_in_ms = crop_start_time_ms < 0 ? 0 :
        (crop_start_time_ms > decoder->duration_ms ? decoder->duration_ms : crop_start_time_ms);
    decoder->crop_end_time_in_ms = crop_end_time_ms < 0 ? 0 :
        (crop_end_time_ms > decoder->duration_ms ? decoder->duration_ms : crop_end_time_ms);
    decoder->duration_ms = decoder->crop_end_time_in_ms - decoder->crop_start_time_in_ms;

    decoder->crop_start_pos = calculate_location_bytes(
        decoder->crop_start_time_in_ms, decoder->bits_per_sample,
        decoder->src_sample_rate_in_Hz, decoder->src_nb_channels);
    decoder->crop_end_pos = calculate_location_bytes(
        decoder->crop_end_time_in_ms, decoder->bits_per_sample,
        decoder->src_sample_rate_in_Hz, decoder->src_nb_channels);
    decoder->duration_bytes = decoder->crop_end_pos - decoder->crop_start_pos;

    LogInfo("%s crop_start_time %d crop_end_time %d duration %d.\n", __func__,
        decoder->crop_start_time_in_ms, decoder->crop_end_time_in_ms,
        decoder->duration_ms);
}

static int init_decoder(IAudioDecoder_Opaque *decoder,
        const char *file_addr, int src_sample_rate, int src_nb_channels,
        int dst_sample_rate, int dst_nb_channels, float volume_flp) {
    LogInfo("%s\n", __func__);
    int ret = -1;
    if (!decoder || !file_addr)
        return ret;

    char *tmp_file_addr = NULL;
    if ((ret = CopyString(file_addr, &tmp_file_addr)) < 0) {
        LogError("%s CopyString to tmp_file_addr failed\n", __func__);
        goto end;
    }

    init_decoder_params(decoder,src_sample_rate, src_nb_channels,
        dst_sample_rate, dst_nb_channels, volume_flp);

    // Allocate buffer for audio fifo
    decoder->pcm_fifo = fifo_create(sizeof(short));
    if (!decoder->pcm_fifo) {
        LogError("%s Could not allocate pcm FIFO\n", __func__);
        ret = AEERROR_NOMEM;
        goto end;
    }

    decoder->max_src_buffer_size = MAX_NB_SAMPLES;
    decoder->src_buffer = (short *)calloc(sizeof(short), decoder->max_src_buffer_size);
    if (!decoder->src_buffer) {
        LogError("%s calloc src_buffer failed.\n", __func__);
        ret = AEERROR_NOMEM;
        goto end;
    }

    if (src_nb_channels != dst_nb_channels) {
        if (src_nb_channels == 1) {
            decoder->max_dst_buffer_size = decoder->max_src_buffer_size << 1;
        } else if (src_nb_channels == 2) {
            decoder->max_dst_buffer_size = decoder->max_src_buffer_size >> 1;
        } else {
            LogError("%s unsupported src_nb_channels %d.\n", __func__, src_nb_channels);
            ret = -1;
            goto end;
        }
        decoder->dst_buffer = (short *)calloc(sizeof(short), decoder->max_dst_buffer_size);
        if (!decoder->dst_buffer) {
            LogError("%s calloc dst_buffer failed.\n", __func__);
            ret = AEERROR_NOMEM;
            goto end;
        }
    }

    if ((ret = CopyString(tmp_file_addr, &decoder->file_addr)) < 0) {
        LogError("%s CopyString failed\n", __func__);
        goto end;
    }

    if ((ret = ae_open_file(&decoder->reader, decoder->file_addr, false)) < 0) {
	LogError("%s read file_addr %s failed\n", __func__, decoder->file_addr);
	goto end;
    }

    get_duration_l(decoder);

    if (tmp_file_addr) {
        av_freep(&tmp_file_addr);
    }
    return 0;
end:
    if (tmp_file_addr) {
        av_freep(&tmp_file_addr);
    }
    PcmDecoder_free(decoder);
    return ret;
}

static void PcmDecoder_free(IAudioDecoder_Opaque *decoder) {
    LogInfo("%s\n", __func__);
    if (NULL == decoder)
        return;

    if (decoder->pcm_fifo) {
        fifo_delete(&decoder->pcm_fifo);
    }
    if (decoder->src_buffer) {
        av_freep(&decoder->src_buffer);
    }
    if (decoder->dst_buffer) {
        av_freep(&decoder->dst_buffer);
    }
    if (decoder->file_addr) {
        av_freep(&decoder->file_addr);
    }
    if (decoder->reader) {
        fclose(decoder->reader);
        decoder->reader = NULL;
    }
}

static int PcmDecoder_get_pcm_frame(
        IAudioDecoder_Opaque *decoder, short *buffer,
        int buffer_size_in_short, bool loop) {
    int ret = -1;
    if (!decoder || !buffer || buffer_size_in_short < 0)
        return ret;
    if (decoder->decode_completed) return PCM_FILE_EOF;

    while (fifo_occupancy(decoder->pcm_fifo) < (size_t) buffer_size_in_short) {
	ret = write_fifo(decoder);
	if (ret < 0) {
	    if (loop && ret == PCM_FILE_EOF) {
	        int crop_start_time_in_ms = decoder->crop_start_time_in_ms;
                int crop_end_time_in_ms = decoder->crop_end_time_in_ms;
	        ret = init_decoder(decoder, decoder->file_addr, decoder->src_sample_rate_in_Hz,
	            decoder->src_nb_channels, decoder->dst_sample_rate_in_Hz,
	            decoder->dst_nb_channels, decoder->volume_flp);
	        if (ret < 0) {
	            LogError("%s init_decoder failed\n", __func__);
	            goto end;
	        }
	        if (PcmDecoder_set_crop_pos(decoder,
                    crop_start_time_in_ms, crop_end_time_in_ms) < 0) {
                    LogError("%s PcmDecoder_set_crop_pos failed.\n", __func__);
                    goto end;
                }
	    } else if (0 < fifo_occupancy(decoder->pcm_fifo)) {
	        break;
	    } else {
	        decoder->decode_completed = true;
	        goto end;
	    }
	}
    }

    return fifo_read(decoder->pcm_fifo, buffer, buffer_size_in_short);
end:
    return ret;
}

static int PcmDecoder_seekTo(IAudioDecoder_Opaque *decoder,
        int seek_pos_ms) {
    LogInfo("%s seek_pos_ms %d\n", __func__, seek_pos_ms);
    if (NULL == decoder)
        return -1;

    decoder->decode_completed = false;
    decoder->seek_pos_ms = seek_pos_ms < 0 ? 0 : seek_pos_ms;
    int file_duration = decoder->duration_ms;
    if (file_duration > 0 && decoder->seek_pos_ms != file_duration) {
        decoder->seek_pos_ms = decoder->seek_pos_ms % file_duration;
    }
    LogInfo("%s decoder->seek_pos_ms %d, file_duration %d\n", __func__,
        decoder->seek_pos_ms, file_duration);

    if (decoder->pcm_fifo) fifo_clear(decoder->pcm_fifo);
    decoder->cur_pos = 0;

    //The offset needs to be a multiple of 2, because the pcm data is 16-bit.
    //The size of seek is in pcm data.
    decoder->seek_pos_bytes = calculate_location_bytes(decoder->seek_pos_ms,
        decoder->bits_per_sample, decoder->src_sample_rate_in_Hz,
        decoder->src_nb_channels);
    LogInfo("%s fseek offset %"PRId64".\n", __func__,
        decoder->seek_pos_bytes + decoder->pcm_start_pos + decoder->crop_start_pos);
    return fseek(decoder->reader,
        decoder->seek_pos_bytes + decoder->pcm_start_pos + decoder->crop_start_pos, SEEK_SET);
}

static int PcmDecoder_set_crop_pos(
    IAudioDecoder_Opaque *decoder, int crop_start_time_ms,
    int crop_end_time_ms) {
    LogInfo("%s\n", __func__);
    int ret = -1;
    if (!decoder || !decoder->reader)
        return ret;

    init_timings_params(decoder, crop_start_time_ms, crop_end_time_ms);

    ret = fseek(decoder->reader,
        decoder->pcm_start_pos + decoder->crop_start_pos, SEEK_SET);

    return ret < 0 ? ret : decoder->duration_ms;
}

IAudioDecoder *PcmDecoder_create(const char *file_addr,
        int src_sample_rate, int src_nb_channels, int dst_sample_rate,
        int dst_nb_channels, float volume_flp) {
    LogInfo("%s.\n", __func__);
    int ret = -1;
    if (!file_addr) {
        LogError("%s file_addr is NULL.\n", __func__);
        return NULL;
    }

    if (src_sample_rate != dst_sample_rate) {
        LogError("%s not support pcm resampling.\n", __func__);
        return NULL;
    }

    IAudioDecoder *decoder = IAudioDecoder_create(sizeof(IAudioDecoder_Opaque));
    if (!decoder) {
        LogError("%s Could not allocate IAudioDecoder.\n", __func__);
        return NULL;
    }

    decoder->func_set_crop_pos = PcmDecoder_set_crop_pos;
    decoder->func_seekTo = PcmDecoder_seekTo;
    decoder->func_get_pcm_frame = PcmDecoder_get_pcm_frame;
    decoder->func_free = PcmDecoder_free;

    IAudioDecoder_Opaque *opaque = decoder->opaque;
    if ((ret = init_decoder(opaque, file_addr, src_sample_rate, src_nb_channels,
        dst_sample_rate, dst_nb_channels, volume_flp)) < 0) {
        LogError("%s init_decoder failed\n", __func__);
        goto end;
    }
    decoder->out_sample_rate = opaque->dst_sample_rate_in_Hz;
    decoder->out_nb_channels = opaque->dst_nb_channels;
    decoder->out_bits_per_sample = opaque->bits_per_sample;
    decoder->duration_ms = opaque->duration_ms;

    return decoder;
end:
    if (decoder) {
        IAudioDecoder_freep(&decoder);
    }
    return NULL;
}
