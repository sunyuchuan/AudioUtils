#include <stdio.h>
#include <sys/time.h>
#include "log.h"
#include "tools/fifo.h"

int main() {
    AeSetLogLevel(LOG_LEVEL_TRACE);
    AeSetLogMode(LOG_MODE_SCREEN);

    int ret = 0;
    size_t buffer_size = 1024;
    short buffer[buffer_size];
    struct timeval start;
    struct timeval end;
    unsigned long timer;
    gettimeofday(&start, NULL);
    fifo *f = fifo_create(sizeof(short));

    for (int i = 0; i < 10; ++i) {
        for (size_t j = 0; j < buffer_size; ++j) {
            buffer[j] = i * j;
        }
        ret = fifo_write(f, buffer, buffer_size);
        if (ret < 0) goto end;

        ret = fifo_write(f, buffer, ret);
        if (ret < 0) goto end;

        while ((ret = fifo_read(f, buffer, buffer_size)) > 0) {
        }
    }

end:
    if (f) fifo_delete(&f);
    gettimeofday(&end, NULL);
    timer = 1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec;
    LogInfo("time consuming %ld us\n", timer);
    return 0;
}