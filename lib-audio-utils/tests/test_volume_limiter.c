#include <sys/time.h>
#include "effects/voice_effect.h"
#include "file_helper.h"
#include "log.h"

#define DEFAULT_SAMPLE_RATE 44100
#define DEFAULT_CHANNEL_NUMBER 1

int main(int argc, char **argv) {
    AeSetLogLevel(LOG_LEVEL_TRACE);
    AeSetLogMode(LOG_MODE_SCREEN);

    if (argc < 2) {
        LogWarning("Usage %s input_pcm_file output_pcm_file\n", argv[0]);
        return 0;
    }

    int ret = 0;
    size_t buffer_size = 1024;
    short buffer[buffer_size];
    FILE *pcm_reader = NULL;
    FILE *pcm_writer = NULL;
    EffectContext *ctx = NULL;
    struct timeval start;
    struct timeval end;
    unsigned long timer;
    gettimeofday(&start, NULL);

    // open input pcm file
    ret = OpenFile(&pcm_reader, argv[1], 0);
    if (ret < 0) goto end;
    // open output pcm file
    ret = OpenFile(&pcm_writer, argv[2], 1);
    if (ret < 0) goto end;

    ctx = create_effect(find_effect("limiter"), DEFAULT_SAMPLE_RATE, DEFAULT_CHANNEL_NUMBER);
    ret = init_effect(ctx, 0, NULL);
    if (ret < 0) goto end;
    set_effect(ctx, "Switch", "On", 0);

    while (buffer_size ==
           fread(buffer, sizeof(short), buffer_size, pcm_reader)) {
        // send data
        ret = send_samples(ctx, buffer, buffer_size);
        if (ret < 0) break;
        // receive data
        ret = receive_samples(ctx, buffer, buffer_size);
        while (ret > 0) {
            fwrite(buffer, sizeof(short), ret, pcm_writer);
            ret = receive_samples(ctx, buffer, buffer_size);
        }
    }

end:
    if (pcm_reader) {
        fclose(pcm_reader);
        pcm_reader = NULL;
    }
    if (pcm_writer) {
        fclose(pcm_writer);
        pcm_writer = NULL;
    }
    if (ctx) {
        free_effect(ctx);
        ctx = NULL;
    }
    gettimeofday(&end, NULL);
    timer = 1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec;
    LogInfo("time consuming %ld us\n", timer);
    return 0;
}

