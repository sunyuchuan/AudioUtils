#include "pitch_cand_loader.h"
#include <stdio.h>
#include "pitch_type.h"

int LoadPitchCandFile(void *pitch_file_handle, PitchFrame **frame_list,
                      int frame_num, int file_size) {
    int i, total_size_count = 0, cand_read_num[2] = {0};
    FILE *file_handle = (FILE *)pitch_file_handle;

    if (pitch_file_handle == NULL) {
        return -1;
    }

    if (frame_list == NULL) {
        return -1;
    }

    for (i = 0; i < frame_num; i++) {
        fread(cand_read_num, sizeof(int), 2, file_handle);
        frame_list[i]->nCandidates = cand_read_num[0];
        frame_list[i]->intensity = *(float *)&cand_read_num[1];
        fread(frame_list[i]->candidate, sizeof(float) * 2, cand_read_num[0],
              file_handle);
        total_size_count += (cand_read_num[0] * 8 + 8);
    }

    if (total_size_count != (file_size - 4)) {
        printf("Pitch cand file size check error!\n");
        return -1;
    }

    return 0;
}
