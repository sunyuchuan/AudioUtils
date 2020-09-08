//
// Created by layne on 19-4-27.
//

#include "fifo.h"
#include <stdlib.h>
#include <string.h>

#define FFMAX(a, b) ((a) > (b) ? (a) : (b))
#define FFMAX3(a, b, c) FFMAX(FFMAX(a, b), c)
#define FFMIN(a, b) ((a) > (b) ? (b) : (a))
#define FFMIN3(a, b, c) FFMIN(FFMIN(a, b), c)

struct fifo_t {
    char *data;
    size_t allocation; /* Number of bytes allocated for data. */
    size_t item_size;  /* Size of each item in data */
    size_t begin;      /* Offset of the first byte to read. */
    size_t end;        /* 1 + Offset of the last byte byte to read. */
};

#define FIFO_MIN 0x4000

void fifo_clear(fifo *f) { f->end = f->begin = 0; }

static void *fifo_reserve(fifo *f, size_t n) {
    n *= f->item_size;

    if (f->begin == f->end) fifo_clear(f);

    while (1) {
        if (f->end + n <= f->allocation) {
            void *p = f->data + f->end;

            f->end += n;
            return p;
        }
        if (f->begin > FIFO_MIN) {
            memmove(f->data, f->data + f->begin, f->end - f->begin);
            f->end -= f->begin;
            f->begin = 0;
            continue;
        }
        f->allocation += n;
        f->data = realloc(f->data, f->allocation);
    }
}

size_t fifo_occupancy(fifo *f) { return (f->end - f->begin) / f->item_size; }

int fifo_read(fifo *f, void *data, const size_t n) {
    if (NULL == f || NULL == data) return -1;
    size_t nb = FFMIN(n, (f->end - f->begin) / f->item_size);
    memcpy(data, f->data + f->begin, nb * f->item_size);
    f->begin += nb * f->item_size;
    return nb;
}

int fifo_write(fifo *f, const void *data, const size_t n) {
    if (NULL == f || NULL == data) return -1;
    void *s = fifo_reserve(f, n);
    memcpy(s, data, n * f->item_size);
    return n;
}

void fifo_delete(fifo **f) {
    if (NULL == f || NULL == *f) return;
    if ((*f)->data) {
        free((*f)->data);
        (*f)->data = NULL;
    }
    free(*f);
    *f = NULL;
}

fifo *fifo_create(size_t item_size) {
    fifo *self = (fifo *)calloc(1, sizeof(fifo));
    if (NULL == self) return NULL;

    self->item_size = item_size;
    self->allocation = FIFO_MIN;
    self->data = malloc(self->allocation);
    if (NULL == self->data)
        fifo_delete(&self);
    else
        fifo_clear(self);
    return self;
}