//
// Created by layne on 19-4-27.
//

#ifndef AUDIO_EFFECT_FIFO_H
#define AUDIO_EFFECT_FIFO_H

#include <stddef.h>

typedef struct fifo_t fifo;

void fifo_clear(fifo *f);
size_t fifo_occupancy(fifo *f);
int fifo_read(fifo *f, void *data, const size_t n);
int fifo_write(fifo *f, const void *data, const size_t n);
void fifo_delete(fifo **f);
fifo *fifo_create(size_t item_size);

#endif  // AUDIO_EFFECT_FIFO_H
