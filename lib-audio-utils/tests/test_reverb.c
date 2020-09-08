#include <sys/time.h>
#include "effects/voice_effect.h"
#include "file_helper.h"
#include "log.h"
#include <stdlib.h>
#include "tools/conversion.h"

int main(int argc, char **argv) {
    AeSetLogLevel(LOG_LEVEL_TRACE);
    AeSetLogMode(LOG_MODE_SCREEN);

    if (argc < 2) {
        LogWarning("Usage %s input_pcm_file output_pcm_file\n", argv[0]);
        return 0;
    }

    int ret = 0;
    size_t buffer_size = 1024;
    short fix_buffer[buffer_size];
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
    ret = OpenFile(&pcm_writer, argv[4], 1);
    if (ret < 0) goto end;

    ctx = create_effect(find_effect("reverb"), atoi(argv[2]),
        atoi(argv[3]));
    ret = init_effect(ctx, 0, NULL);
    if (ret < 0) goto end;

    ret = set_effect(ctx, "Switch", "On", 0);
    if (ret < 0) goto end;

    ret = set_effect(ctx, "reverb", REVERB_PARAMS, 0);
    if (ret < 0) goto end;

    while (buffer_size ==
            fread(fix_buffer, sizeof(short), buffer_size, pcm_reader)) {
        // send data
        ret = send_samples(ctx, fix_buffer, buffer_size);
        if (ret < 0) break;

        // receive data
        ret = receive_samples(ctx, fix_buffer, buffer_size);
        while (ret > 0) {
            fwrite(fix_buffer, sizeof(short), ret, pcm_writer);
            ret = receive_samples(ctx, fix_buffer, buffer_size);
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
