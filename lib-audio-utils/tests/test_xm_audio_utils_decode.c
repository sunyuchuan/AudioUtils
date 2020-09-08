#include "xm_audio_utils.h"
#include "codec/ffmpeg_utils.h"
#include <sys/time.h>
#include <stdlib.h>
#include "error_def.h"
#include "file_helper.h"
#include "log.h"
#include "xm_duration_parser.h"

static inline int calculation_duration_ms(int64_t size,
    float bytes_per_sample, int nb_channles, int sample_rate) {
    return 1000 * (size / bytes_per_sample / nb_channles / sample_rate);
}

int main(int argc, char **argv) {
    AeSetLogLevel(LOG_LEVEL_TRACE);
    AeSetLogMode(LOG_MODE_SCREEN);

    for (int i = 0; i < argc; i++) {
        LogInfo("argv[%d] %s\n", i, argv[i]);
    }

    int ret = 0;
    int buffer_size_in_short = 1024;
    short *buffer = NULL;
    struct timeval start;
    struct timeval end;
    unsigned long timer;
    gettimeofday(&start, NULL);

    FILE *pcm_writer = NULL;
    ret = OpenFile(&pcm_writer, argv[2], true);
    if (ret < 0) {
        LogError("OpenFile %s failed\n", argv[4]);
        goto end;
    }

    buffer = (short *)calloc(sizeof(short), buffer_size_in_short);
    if (!buffer) goto end;

    // Set Log
    RegisterFFmpeg();
    XmAudioUtils *utils = xm_audio_utils_create();
    if (utils == NULL) {
        LogError("xm_audio_utils_create failed\n");
        goto end;
    }

    int crop_start_time = 0;
    int crop_end_time = 0;
    ret = get_file_duration_ms(argv[1], false, 0, 0, 0);
    if (ret > 0) {
        crop_end_time = ret /2;
    }

    ret = xm_audio_utils_decoder_create(utils, argv[1], crop_start_time,
        crop_end_time, atoi(argv[3]), atoi(argv[4]), 100);
    if (ret < 0) {
        LogError("xm_audio_utils_decoder_create failed\n");
        goto end;
    }

    xm_audio_utils_decoder_seekTo(utils, 1000);

    ret = xm_audio_utils_fade_init(utils, atoi(argv[3]), atoi(argv[4]), 0, crop_end_time, 5000, 5000);
    if (ret < 0) {
        LogError("xm_audio_utils_fade_init failed\n");
        goto end;
    }

    int64_t cur_size = 0;
    while (1) {
        ret = xm_audio_utils_get_decoded_frame(utils, buffer, buffer_size_in_short, false);
        if (ret <= 0) break;
        int buffer_start_time = calculation_duration_ms(cur_size*sizeof(short),
            16/8, atoi(argv[4]), atoi(argv[3]));
        cur_size += ret;
        xm_audio_utils_fade(utils, buffer, ret, buffer_start_time);
        fwrite(buffer, sizeof(short), ret, pcm_writer);
    }

end:
    if (buffer) {
        free(buffer);
        buffer = NULL;
    }
    if (pcm_writer) {
        fclose(pcm_writer);
        pcm_writer = NULL;
    }
    xm_audio_utils_freep(&utils);

    gettimeofday(&end, NULL);
    timer = 1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec;
    printf("time consuming %ld us\n", timer);
    return 0;
}

