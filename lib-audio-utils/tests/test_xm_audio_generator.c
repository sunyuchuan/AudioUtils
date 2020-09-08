#include "xm_audio_generator.h"
#include "codec/ffmpeg_utils.h"
#include <sys/time.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include "error_def.h"
#include "log.h"

#define ENCODER_FFMPEG 0
#define ENCODER_MEDIA_CODEC 1
static volatile bool abort_request = false;

void *get_progress(void *arg) {
    int progress = 0;
    XmAudioGenerator *ctx = arg;
    while (!abort_request) {
        usleep(100000);
        progress = xm_audio_generator_get_progress(ctx);
        LogInfo("%s get_progress : %d\n", __func__, progress);
    }
    return NULL;
}

int main(int argc, char **argv) {
    struct timeval start;
    struct timeval end;
    unsigned long timer;
    gettimeofday(&start, NULL);

    AeSetLogLevel(LOG_LEVEL_TRACE);
    AeSetLogMode(LOG_MODE_SCREEN);

    for (int i = 0; i < argc; i++) {
        LogInfo("argv[%d] %s\n", i, argv[i]);
    }

    XmAudioGenerator *generator = xm_audio_generator_create();
    if (!generator) {
        LogError("%s xm_audio_generator_create failed\n", __func__);
        goto end;
    }

    pthread_t get_progress_tid = 0;
    if (pthread_create(&get_progress_tid, NULL, get_progress, generator)) {
        LogError("Error:unable to create get_progress thread\n");
        goto end;
    }

    enum GeneratorStatus ret = xm_audio_generator_start(
        generator, argv[1], argv[2], ENCODER_FFMPEG);
    if (ret == GS_ERROR) {
	LogError("%s xm_audio_generator_start failed\n", __func__);
	goto end;
    }

end:
    abort_request = true;
    pthread_join(get_progress_tid, NULL);
    xm_audio_generator_stop(generator);
    xm_audio_generator_freep(&generator);
    gettimeofday(&end, NULL);
    timer = 1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec;
    LogInfo("time consuming %ld us\n", timer);

    return 0;
}

