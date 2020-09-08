#include <sys/time.h>
#include "effects/voice_effect.h"
#include "file_helper.h"
#include "log.h"
#include "tools/conversion.h"

int main(int argc, char **argv) {
    AeSetLogLevel(LOG_LEVEL_TRACE);
    AeSetLogMode(LOG_MODE_SCREEN);

    if (argc < 3) {
        LogWarning("Usage %s input_pcm_file output_pcm_file effect_name\n",
                   argv[0]);
        return 0;
    }

    int ret = 0;
    size_t loop_times = 0;
    size_t buffer_size = 1024;
    short fix_buffer[buffer_size];
    float flp_buffer[buffer_size];
    FILE *pcm_reader = NULL;
    FILE *pcm_writer = NULL;
    EffectContext *ctx = NULL;
    struct timeval start;
    struct timeval end;
    unsigned long timer;
    gettimeofday(&start, NULL);

    // open input and output file
    ret = OpenFile(&pcm_reader, argv[1], 0);
    if (ret < 0) goto end;
    ret = OpenFile(&pcm_writer, argv[2], 1);
    if (ret < 0) goto end;

    // create effects
    ctx = create_effect(find_effect(argv[3]), 44100, 1);
    ret = init_effect(ctx, argc - 3, (const char **)(argv + 3));
    if (ret < 0) goto end;

    while (buffer_size ==
           fread(fix_buffer, sizeof(short), buffer_size, pcm_reader)) {
        S16ToFloat(fix_buffer, flp_buffer, buffer_size);
        // send data
        ret = send_samples(ctx, flp_buffer, buffer_size);
        if (ret < 0) break;

        // receive data
        ret = receive_samples(ctx, flp_buffer, buffer_size);
        while (ret >= buffer_size) {
            FloatToS16(flp_buffer, fix_buffer, ret);
            fwrite(fix_buffer, sizeof(short), buffer_size, pcm_writer);
            ret = receive_samples(ctx, flp_buffer, buffer_size);
        }
        if (100 == loop_times) {
            ret = set_effect(ctx, "Switch", "Off", 0);
            if (ret < 0) goto end;
        } else if (200 == loop_times) {
            ret = set_effect(ctx, "echo", "0.8 0.9 1000 0.3 1800 0.25", 0);
            if (ret < 0) goto end;
            ret = set_effect(ctx, "echos", "0.8 0.7 40 0.25 63 0.3", 0);
            if (ret < 0) goto end;
            ret = set_effect(ctx, "reverb", "50.0 50.0 100.0 0.0 5.0", 0);
            if (ret < 0) goto end;
        }
        ++loop_times;
    }

end:
    // free
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