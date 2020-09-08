#include <sys/time.h>
#include "codec/audio_muxer.h"
#include "error_def.h"
#include "log.h"
#include "codec/audio_decoder_factory.h"
#include "xm_duration_parser.h"

#define DEFAULT_SAMPLE_RATE 44100
#define DEFAULT_CHANNEL_NUMBER 2
#define MIME_AUDIO_AAC "audio/aac"

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

    int crop_start_time = 0;
    int crop_end_time = 0;
    int sample_rate = atoi(argv[2]);
    int nb_channels = atoi(argv[3]);

    ret = get_file_duration_ms(argv[1], true, 16, sample_rate, nb_channels);
    if (ret > 0) {
        crop_end_time = ret/2;
    }

    IAudioDecoder *decoder = audio_decoder_create(argv[1], sample_rate, nb_channels,
        sample_rate, nb_channels, 1.0f, DECODER_PCM);
    if (decoder == NULL) {
        LogError("audio_decoder_create failed\n");
        goto end;
    }

    if (IAudioDecoder_set_crop_pos(decoder, crop_start_time, crop_end_time) < 0) {
        LogError("%s IAudioDecoder_set_crop_pos failed.\n", __func__);
        goto end;
    }

    buffer = (short *)calloc(sizeof(short), buffer_size_in_short);
    if (!buffer) goto end;

    RegisterFFmpeg();
    MuxerConfig config;
    config.src_sample_rate_in_Hz = atoi(argv[2]);
    config.src_nb_channels = atoi(argv[3]);
    config.dst_sample_rate_in_Hz = DEFAULT_SAMPLE_RATE;
    config.dst_nb_channels = DEFAULT_CHANNEL_NUMBER;
    config.mime = MIME_AUDIO_AAC;
    config.muxer_name = MUXER_AUDIO_MP4;
    config.output_filename = argv[4];
    config.src_sample_fmt = AV_SAMPLE_FMT_S16;
    config.codec_id = AV_CODEC_ID_AAC;
    config.encoder_type = ENCODER_FFMPEG;
    AudioMuxer *muxer = muxer_create(&config);
    if (muxer == NULL) {
        LogError("muxer_create failed\n");
        goto end;
    }

    while (1) {
        ret = IAudioDecoder_get_pcm_frame(decoder, buffer, buffer_size_in_short, false);
        if (ret <= 0) {
            LogInfo("ret <= 0, EOF, exit\n");
            break;
        }
        ret = muxer_write_audio_frame(muxer, buffer, ret);
        if (ret < 0) {
            LogError("muxer_write_audio_frame failed\n");
            break;
        }
    }

end:
    if (buffer) {
        free(buffer);
        buffer = NULL;
    }
    if (decoder) {
        IAudioDecoder_freep(&decoder);
    }
    muxer_stop(muxer);
    muxer_freep(&muxer);

    gettimeofday(&end, NULL);
    timer = 1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec;
    printf("time consuming %ld us\n", timer);
    return 0;
}

