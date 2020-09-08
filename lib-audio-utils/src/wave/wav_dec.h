#ifndef WAV_DEC_H
#define WAV_DEC_H
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define TAG_ID_RIFF "RIFF"
#define FILE_FORM_WAV "WAVE"
#define TAG_ID_FMT "fmt "
#define TAG_ID_DATA "data"

typedef struct WavHeader {
    unsigned char riff_id[4];
    uint32_t riff_size;
    unsigned char form[4];
    unsigned char fmt_id[4];
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t nb_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    unsigned char data_id[4];
    uint32_t data_size;
} WavHeader;

typedef struct WavContext {
    bool is_wav;
    uint32_t file_size;
    uint32_t pcm_data_offset;
    WavHeader header;
} WavContext;

int wav_write_header(FILE *writer, WavContext *ctx);
int wav_read_header(const char *file_addr, WavContext *ctx);

#endif // WAV_DEC_H
