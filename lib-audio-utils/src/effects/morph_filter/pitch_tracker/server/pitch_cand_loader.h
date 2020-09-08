#ifndef _PITCH_CANDIDATE_FILE_LOADER_H_
#define _PITCH_CANDIDATE_FILE_LOADER_H_

#include "pitch_type.h"
int LoadPitchCandFile(void* pitch_file_handle, PitchFrame** frame_list,
                      int frame_num, int file_size);

#endif