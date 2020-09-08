#include <sys/time.h>
#include "codec/audio_decoder_factory.h"
#include "error_def.h"
#include "file_helper.h"
#include "log.h"
#include "codec/ffmpeg_utils.h"

int main(int argc, char **argv) {
    AeSetLogLevel(LOG_LEVEL_TRACE);
    AeSetLogMode(LOG_MODE_SCREEN);
    if (argc < 3) {
        LogError("Usage %s input_decode_file output_pcm_file\n", argv[0]);
        return 0;
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
        LogError("OpenFile %s failed\n", argv[2]);
        goto end;
    }

    buffer = (short *)calloc(sizeof(short), buffer_size_in_short);
    if (!buffer) goto end;

    // Set Log
    RegisterFFmpeg();
    IAudioDecoder *decoder = audio_decoder_create(argv[1], 0, 0,
        atoi(argv[3]), atoi(argv[4]), 1.0f, DECODER_FFMPEG);
    if (decoder == NULL) {
        LogError("audio_decoder_create failed\n");
        goto end;
    }

    while (1) {
        ret = IAudioDecoder_get_pcm_frame(decoder, buffer, buffer_size_in_short, false);
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
    IAudioDecoder_freep(&decoder);

    gettimeofday(&end, NULL);
    timer = 1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec;
    printf("time consuming %ld us\n", timer);
    return 0;
}
