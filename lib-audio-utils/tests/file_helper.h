//
// Created by layne on 19-4-28.
//

#ifndef AUDIO_EFFECT_FILE_HELPER_H_
#define AUDIO_EFFECT_FILE_HELPER_H_

#include <stdio.h>

static int OpenFile(FILE **fp, const char *file_name, const int is_write) {
    int ret = 0;
    if (*fp) {
        fclose(*fp);
        *fp = NULL;
    }
    if (is_write)
        *fp = fopen(file_name, "wb");
    else
        *fp = fopen(file_name, "rb");
    if (!*fp) {
        fprintf(stderr, "Can not open file %s\n", file_name);
        ret = -1;
    }
    return ret;
}

#endif  // AUDIO_EFFECT_FILE_HELPER_H_
