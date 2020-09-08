/*
 * ff_encoder.c
 *
 * Copyright (c) 2003 Fabrice Bellard
 * Copyright (c) 2014 Zhang Rui <bbcallen@gmail.com>
 *
 * This file is part of ijkPlayer.
 *
 * ijkPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * ijkPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with ijkPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#if defined(__ANDROID__) || defined (__linux__)

#include "audio_encoder.h"
#include <stdlib.h>
#include <string.h>

Encoder *ff_encoder_alloc(size_t opaque_size)
{
    Encoder *encoder = (Encoder*) calloc(1, sizeof(Encoder));
    if (!encoder)
        return NULL;

    encoder->opaque = calloc(1, opaque_size);
    if (!encoder->opaque) {
        free(encoder);
        return NULL;
    }

    return encoder;
}

void ff_encoder_free(Encoder *encoder)
{
    if (!encoder)
        return;

    if (encoder->func_destroy) {
        encoder->func_destroy(encoder);
    }

    free(encoder->opaque);
    memset(encoder, 0, sizeof(Encoder));
    free(encoder);
}

void ff_encoder_free_p(Encoder **encoder)
{
    if (!encoder)
        return;

    ff_encoder_free(*encoder);
    *encoder = NULL;
}

int ff_encoder_config(Encoder *encoder, AVDictionary *opt)
{
    if (!encoder || !encoder->func_config)
        return 0;

    return encoder->func_config(encoder, opt);
}

int ff_encoder_get_frame_size(Encoder *encoder)
{
    if (!encoder || !encoder->func_get_frame_size)
        return 0;

    return encoder->func_get_frame_size(encoder);
}

int ff_encoder_encode_frame(Encoder *encoder, AVFrame *frame, AVPacket *pkt, int *got_packet_ptr)
{
    if (!encoder || !encoder->func_encode_frame)
        return 0;

    return encoder->func_encode_frame(encoder, frame, pkt, got_packet_ptr);
}
#endif
