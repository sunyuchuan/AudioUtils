#include <stdio.h>
#include "pitch_cand_loader.h"
#include "pitch_find_path.h"
#include <stdlib.h>
#include <string.h>

#define MAX_PITCH_CAND_NUM 10

int main(int argc, char *argv[]) {
    FILE *fid_cand, *fid_best_seq;
    int frame_num, i, j, file_size;
    PitchFrame **pitch_frame_list;
    PitchCandidate *tcand;
    float silence_thrd = 0.01f, voicing_thrd = 0.25f, octave_cost = 0.1f,
          octave_jump_cost = 0.75f, voiced_unvoiced_cost = 0.1f,
          ceiling = 450.0f;

    /*int argc = 3;
    char *argv[] =
    {"xmlyVoiceMorph.exe","../xmlyVoiceMorph/test","../xmlyVoiceMorph/best_sequence"};*/
    ////char *argv[] = {"-help"};

    if (argc == 2) {
        if (strcmp(argv[1], "-help") == 0) {
            printf(
                "Usage: PitchServer cand_file_path best_sequence_output_path\n"
                "1.cand_file_path - Input Pitch Candidate File Absolute Path "
                "Including File Name.\n"
                "2.best_sequence_output_path - Best Pitch Sequence File "
                "Absolute Path Including File Name.\n");
        } else {
            printf("Too few input arguments!\n");
            return -1;
        }
    } else if (argc == 1) {
        printf(
            "Too few arguments, please use '-help' for further information.\n");
        return -1;
    } else if (argc > 3) {
        printf("Too many input arguments!\n");
    } else if (argc < 3) {
        printf("Too few input arguments!\n");
    } else if (argc == 3) {
        fid_cand = fopen(argv[1], "rb+");
        if (fid_cand == NULL) {
            printf(
                "Can't open input pitch candidate file. Please refer to "
                "argument 1.\n");
            return -1;
        }

        fid_best_seq = fopen(argv[2], "wb+");
        if (fid_best_seq == NULL) {
            printf(
                "Can't open output pitch file. Please refer to argument 2.\n");
            return -1;
        }

        if (fseek(fid_cand, -4, SEEK_END) != 0) {
            printf("Seek file error.\n");
            return -1;
        }

        if (fread(&frame_num, sizeof(int), 1, fid_cand) != 1) {
            printf("Can't get pitch candidate number.\n");
            return -1;
        }

        file_size = ftell(fid_cand);

        if (fseek(fid_cand, 0, SEEK_SET) != 0) {
            printf("Seek file error.\n");
            return -1;
        }

        if (frame_num > 0) {
            pitch_frame_list =
                (PitchFrame **)malloc(sizeof(PitchFrame *) * frame_num);
            if (pitch_frame_list == NULL) {
                printf("Can't allocate memory for pitch frame.\n");
                return -1;
            }

            for (i = 0; i < frame_num; i++) {
                pitch_frame_list[i] = (PitchFrame *)malloc(sizeof(PitchFrame));
                if (pitch_frame_list[i] == NULL) {
                    printf("Can't allocate memory for pitch candidates.\n");
                    return -1;
                }
                pitch_frame_list[i]->nCandidates = 0;
                pitch_frame_list[i]->intensity = 0.0f;
                pitch_frame_list[i]->candidate = (PitchCandidate *)malloc(
                    sizeof(PitchCandidate) * MAX_PITCH_CAND_NUM);
                if (pitch_frame_list[i]->candidate == NULL) {
                    printf("Can't allocate memory for pitch candidates.\n");
                    return -1;
                }
            }

            for (i = 0; i < frame_num; i++) {
                tcand = pitch_frame_list[i]->candidate;
                for (j = 0; j < MAX_PITCH_CAND_NUM; j++) {
                    tcand[j].frequency = 0.0f;
                    tcand[j].strength = 0.0f;
                }
            }

            if (LoadPitchCandFile(fid_cand, pitch_frame_list, frame_num,
                                  file_size) != 0) {
                printf("Can't load pitch candidate file.\n");
                return -1;
            }

            if (PitchPathFinder(pitch_frame_list, silence_thrd, voicing_thrd,
                                octave_cost, octave_jump_cost,
                                voiced_unvoiced_cost, ceiling,
                                frame_num) != 0) {
                printf("Can't find pitch path.\n");
                return -1;
            }

            for (i = 0; i < frame_num; i++) {
                tcand = pitch_frame_list[i]->candidate;
                fprintf(fid_best_seq, "%f\n", tcand[0].frequency);
            }

            for (i = 0; i < frame_num; i++) {
                free(pitch_frame_list[i]->candidate);
                free(pitch_frame_list[i]);
            }
            free(pitch_frame_list);

            fclose(fid_cand);
            fclose(fid_best_seq);

        } else {
            printf(
                "Invalid frame number. Pitch cand file may be broken during "
                "transmission.\n");
            return -1;
        }
    }
}
