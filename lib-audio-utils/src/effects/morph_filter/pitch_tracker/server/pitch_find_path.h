#ifndef _PITCH_FIND_PATH_H_
#define _PITCH_FIND_PATH_H_

#include "pitch_type.h"
int PitchPathFinder(PitchFrame **frame_list, float silenceThreshold,
                    float voicingThreshold, float octaveCost,
                    float octave_jump_cost, float voiced_unvoiced_cost,
                    float ceiling, int frame_num);

#endif