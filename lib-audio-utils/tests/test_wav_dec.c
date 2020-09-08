#include <stdio.h>
#include <sys/time.h>
#include "log.h"
#include "wave/wav_dec.h"

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

    WavContext wav_ctx;
    if (wav_read_header(argv[1], &wav_ctx) < 0) {
        LogError("%s wav_read_header failed\n", __func__);
        goto end;
    }

end:
    gettimeofday(&end, NULL);
    timer = 1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec;
    LogInfo("time consuming %ld us\n", timer);

    return 0;
}

