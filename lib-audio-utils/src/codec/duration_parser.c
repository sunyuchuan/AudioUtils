#include "duration_parser.h"
#include "idecoder.h"
#include "log.h"
#include "error_def.h"
#include "tools/util.h"
#include "ffmpeg_utils.h"
#include <stdio.h>

#define fftime_to_milliseconds(ts) (av_rescale(ts, 1000, AV_TIME_BASE))

extern void RegisterFFmpeg();

int get_audio_file_duration_ms(const char *file_addr) {
    int ret = -1;
    if (!file_addr) return -1;
    LogInfo("%s file_addr %s.\n", __func__, file_addr);

    RegisterFFmpeg();

    AVFormatContext *fmt_ctx = NULL;
    ret = OpenInputMediaFile(&fmt_ctx, file_addr);
    if (ret < 0) goto end;

    int audio_stream_index = FindBestStream(fmt_ctx, AVMEDIA_TYPE_AUDIO);
    ret = audio_stream_index;
    if (ret < 0) goto end;

    AVStream *audio_stream = fmt_ctx->streams[audio_stream_index];
    if (audio_stream->duration != AV_NOPTS_VALUE) {
        ret = av_rescale_q(audio_stream->duration,
            audio_stream->time_base, AV_TIME_BASE_Q) / 1000;
    } else {
        ret = fftime_to_milliseconds(fmt_ctx->duration);
    }

end:
    if (fmt_ctx != NULL) {
        avformat_close_input(&fmt_ctx);
        fmt_ctx = NULL;
    }
    return ret;
}

int get_pcm_file_duration_ms(const char *file_addr,
    int bits_per_sample, int src_sample_rate_in_Hz, int src_nb_channels) {
    int ret = -1;
    if (!file_addr) return -1;
    LogInfo("%s file_addr %s.\n", __func__, file_addr);

    FILE *reader = NULL;
    if ((ret = ae_open_file(&reader, file_addr, false)) < 0) {
        LogError("%s open file_addr %s failed\n", __func__, file_addr);
        goto end;
    }

    fseek(reader, 0, SEEK_END);
    ret = calculation_duration_ms(ftell(reader),
        bits_per_sample/8, src_nb_channels, src_sample_rate_in_Hz);
end:
    if (reader != NULL) fclose(reader);
    return ret;
}
