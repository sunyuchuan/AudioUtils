#include "xm_audio_utils.h"
#include "codec/ffmpeg_utils.h"
#include <sys/time.h>
#include <stdlib.h>
#include "error_def.h"
#include "file_helper.h"
#include "log.h"

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
    ret = OpenFile(&pcm_writer, argv[3], true);
    if (ret < 0) {
        LogError("OpenFile %s failed\n", argv[3]);
        ret = -1;
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

    ret = xm_audio_utils_mixer_init(utils, argv[1]);
    if (ret < 0) {
        LogError("xm_audio_utils_mixer_init failed\n");
        goto end;
    }

    ret = xm_audio_utils_mixer_seekTo(utils, 11000);
    if (ret < 0) {
        LogError("xm_audio_utils_mixer_seekTo failed\n");
        goto end;
    }

    int64_t cur_size = 0;
    while (1) {
	ret = xm_audio_utils_mixer_get_frame(utils, buffer, buffer_size_in_short);
	if (ret <= 0) break;
	fwrite(buffer, sizeof(short), ret, pcm_writer);
	cur_size += ret;
	if (calculation_duration_ms(cur_size*sizeof(short), 16/8,
	    2, atoi(argv[2])) > 33000)
	    break;
    }

    ret = xm_audio_utils_mixer_seekTo(utils, 150226);
    if (ret < 0) {
	LogError("xm_audio_utils_mixer_seekTo failed\n");
	goto end;
    }

    while (1) {
        ret = xm_audio_utils_mixer_get_frame(utils, buffer, buffer_size_in_short);
        if (ret <= 0) break;
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

