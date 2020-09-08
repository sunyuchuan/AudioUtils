#include <stdio.h>
#include <sys/time.h>
#include "log.h"
#include "xm_wav_utils.h"

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

    long duration = xm_wav_utils_get_duration(argv[1]);
    if (duration < 0) {
        LogError("%s xm_wav_utils_get_duration failed\n", __func__);
        goto end;
    }

    if(!xm_wav_utils_crop(argv[1], duration/2, duration, argv[2]))
    {
        LogError("%s wav_read_header failed\n", __func__);
        goto end;
    }

end:
    gettimeofday(&end, NULL);
    timer = 1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec;
    LogInfo("time consuming %ld us\n", timer);

    return 0;
}

