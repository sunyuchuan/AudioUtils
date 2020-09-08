#include "wav_dec.h"
#include "log.h"
#include "tools/util.h"
#include <string.h>

#define FILE_EOF -1000
#define MKTAG(a,b,c,d) ((a) | ((b) << 8) | ((c) << 16) | ((unsigned)(d) << 24))

static uint8_t avio_r8(FILE *reader)
{
    if (!reader) {
        return 0;
    }

    uint8_t buf[1];
    int read_len = fread(buf, 1, 1, reader);
    if (read_len <= 0) {
        return 0;
    }

    return buf[0];
}

static uint16_t avio_rl16(FILE *reader)
{
    uint16_t val;
    val = avio_r8(reader);
    val |= avio_r8(reader) << 8;
    return val;
}

static uint32_t avio_rl32(FILE *reader)
{
    uint32_t val;
    val = avio_rl16(reader);
    val |= avio_rl16(reader) << 16;
    return val;
}

static uint64_t avio_rl64(FILE *reader)
{
    uint64_t val;
    val = (uint64_t)avio_rl32(reader);
    val |= (uint64_t)avio_rl32(reader) << 32;
    return val;
}

static uint32_t next_tag(FILE *reader, uint32_t *tag)
{
    *tag = avio_rl32(reader);
    return avio_rl32(reader);
}

static int wav_parse_fmt_tag(FILE *reader,
    int64_t size, WavHeader *header)
{
    if (!reader || !header) {
        return -1;
    }

    header->audio_format = avio_rl16(reader);
    // 1(0x0001) means PCM data
    if (header->audio_format == 0x0001) {
        header->nb_channels = avio_rl16(reader);
        header->sample_rate = avio_rl32(reader);
        header->byte_rate = avio_rl32(reader);
        header->block_align = avio_rl16(reader);
        LogInfo("%s nb_channels %d, sample_rate %d.\n", __func__,
            header->nb_channels, header->sample_rate);
    } else {
        LogError("%s audio_format not pcm.\n", __func__);
        return -1;
    }

    if (size == 14) {
        header->bits_per_sample = 8;
    } else {
        header->bits_per_sample = avio_rl16(reader);
        LogInfo("%s bits_per_sample %d.\n", __func__,
            header->bits_per_sample);
    }

    return 0;
}

int wav_write_header(FILE *writer, WavContext *ctx)
{
    int ret = -1;
    if (!writer || !ctx) {
        return -1;
    }

    ctx->pcm_data_offset = 0;
    if (!ctx->is_wav) {
        LogWarning("%s is not wav file.\n", __func__);
        return 0;
    }

    if ((ret = fseek(writer, 0, SEEK_SET)) < 0) {
        LogError("%s seek to 0 failed\n", __func__);
        goto end;
    }

    memcpy(ctx->header.riff_id, TAG_ID_RIFF, 4);
    memcpy(ctx->header.form, FILE_FORM_WAV, 4);
    memcpy(ctx->header.fmt_id, TAG_ID_FMT, 4);
    ctx->header.fmt_size= 16;
    memcpy(ctx->header.data_id, TAG_ID_DATA, 4);
    if ((ret = fwrite(&ctx->header, sizeof(ctx->header), 1, writer)) != 1) {
        LogError("%s write wav header failed, ret %d.\n", __func__, ret);
        ret = -1;
        goto end;
    }

    ctx->pcm_data_offset = sizeof(ctx->header);
    ret = 0;
end:
    return ret;
}

int wav_read_header(const char *file_addr, WavContext *ctx)
{
    int got_fmt, ret = -1;
    uint32_t tag = 0;
    uint32_t size = 0, next_tag_ofs = 0;
    WavHeader *header;
    if (!file_addr || !ctx) {
        return -1;
    }

    header = &ctx->header;
    memset(header, 0, sizeof(WavHeader));
    FILE *reader = NULL;
    if ((ret = ae_open_file(&reader, file_addr, false)) < 0) {
	LogError("%s open file_addr %s failed\n", __func__, file_addr);
	goto fail;
    }
    fseek(reader, 0, SEEK_END);
    ctx->file_size = ftell(reader);
    fseek(reader, 0, SEEK_SET);

    if (feof(reader) || ferror(reader)) {
        LogInfo("%s 1 EOF or read file error.\n", __func__);
        ret = FILE_EOF;
        goto fail;
    }

    ctx->is_wav = false;
    tag = avio_rl32(reader);
    if (tag == MKTAG('R', 'I', 'F', 'F')) {
        header->riff_size = avio_rl32(reader);
        if (avio_rl32(reader) != MKTAG('W', 'A', 'V', 'E')) {
            LogError("%s file form not WAVE.\n", __func__);
            ret = -1;
            goto fail;
        }
        LogInfo("%s file is wav.\n", __func__);
    } else {
        LogInfo("%s file id not RIFF.\n", __func__);
        ret = -1;
        goto fail;
    }

    got_fmt = 0;
    for (;;) {
        size = next_tag(reader, &tag);
        next_tag_ofs = ftell(reader) + size;
        if (feof(reader) || ferror(reader)) {
            LogInfo("%s 2 EOF or read file error.\n", __func__);
            ret = FILE_EOF;
            goto fail;
        }

        switch (tag) {
        case MKTAG('f', 'm', 't', ' '):
            header->fmt_size = size;
            if (!got_fmt && (ret = wav_parse_fmt_tag(reader, size, header)) < 0) {
                LogError("%s get fmt failed.\n", __func__);
                ret = -1;
                goto fail;
            } else if (got_fmt)
                LogWarning("%s Found more than one 'fmt ' tag.\n", __func__);
            LogInfo("%s find fmt tag.\n", __func__);
            got_fmt = 1;
            break;
        case MKTAG('d', 'a', 't', 'a'):
            header->data_size = size;
            ctx->pcm_data_offset = ftell(reader);
            LogInfo("%s find data tag, data offset 0x%x, data size 0x%x.\n", __func__,
                ctx->pcm_data_offset, header->data_size);
            if (header->riff_size + 8 - header->data_size != ctx->pcm_data_offset) {
                LogWarning("%s pcm_data_offset != file_size - data_size, riff_size 0x%x.\n", __func__,
                    header->riff_size);
            }
            ret = 0;
            goto end;
        case MKTAG('X', 'M', 'A', '2'):
        case MKTAG('f', 'a', 'c', 't'):
        case MKTAG('b', 'e', 'x', 't'):
        case MKTAG('S','M','V','0'):
        case MKTAG('L', 'I', 'S', 'T'):
        case MKTAG('J', 'U', 'N', 'K'):
        case MKTAG('F', 'L', 'L', 'R'):
        default:
            break;
        }

        /* seek to next tag unless we know that we'll run into EOF */
        if (fseek(reader, (long)next_tag_ofs, SEEK_SET) < 0) {
            LogError("%s seek to next tag failed.\n", __func__);
            ret = -1;
            goto fail;
        }
    }

end:
    if (header->data_size == 0) {
        header->data_size = ctx->file_size - ctx->pcm_data_offset;
        header->riff_size = header->data_size + ctx->pcm_data_offset - 8;
    }
    ctx->is_wav = true;
    ret = 0;

fail:
    if (reader != NULL) fclose(reader);
    return ret;
}

