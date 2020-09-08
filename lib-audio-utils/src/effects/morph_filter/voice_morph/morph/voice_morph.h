#ifndef _VOICE_MORPH_H_
#define _VOICE_MORPH_H_

#include "libswresample/swresample.h"
#include "pitch_tracker/src/pitch_tracker.h"
#include "morph_core.h"
#include <stdio.h>
#include <stdbool.h>

typedef struct VoiceMorph {
    PitchTracker *pitch;
    float *morph_inbuf_float;
    short morph_buf_pos;
    float *morph_buf;
    short prev_peak_pos;
	float t_prev_peak_pos;

    short src_acc_pos;
	float t_src_acc_pos;
    float *seg_pitch_primary;
    float *seg_pitch_new;
    float seg_pitch_transform;
    float formant_ratio;
    float pitch_ratio;
    float pitch_range_factor;
    short *pitch_peak;
    float *last_joint_buf;
    float *last_fall_buf;
    short last_fall_len;
    float *out_buf;
    short out_len;
    struct SwrContext *swr_ctx;
    unsigned char **rsmp_in_buf;
    unsigned char **rsmp_out_buf;
    int rsmp_in_len_count;
    int rsmp_out_len;
    int rsmp_out_linesize;
    int rate_supervisor;
    int resample_initialized;
    refactor rebuild_factor;
	float morph_factor;
	bool robot_status;
} VoiceMorph;

/**************************************************************************
 *Function  -   allocate space for morph instance
 *
 *Input		-	file_name	-	file address to write
 *pitch Output	-	None
 *
 *Return	-	NULL - fail
 **************************************************************************/
VoiceMorph* VoiceMorph_Create(char *file_name);

/**************************************************************************
 *Function  -   initialize morph instance
 *
 *Input		-	None
 *Output	-	None
 *
 *Return	-	-1 - fail
 *				0  - succeed
 **************************************************************************/
int VoiceMorph_Init(VoiceMorph *self);

/**************************************************************************
 *Function  -   set parameters for morph instance
 *
 *Input		-	pitch_coeff: pitch rise/fall extent
 *Output	-	None
 *
 *Return	-	-1 - fail
 *				0  - succeed
 *Note		-	this function is called when user change morph
 *setting parameters.
 **************************************************************************/
int VoiceMorph_SetConfig(VoiceMorph *self, float pitch_coeff);

/**************************************************************************
 *Function  -   process a block pcm for morph
 *
 *Input		-	raw	-	input data address
 *				in_size	-	input data size in byte
 *				robot - true if robot voice morph is on; false
 *norm voice morph; Output	-	morph_out	-	output address
 *in byte morph_out_size	-	output data size in byte
 *
 *Return	-	-1 - fail
 *				0  - succeed
 **************************************************************************/
int VoiceMorph_Process(VoiceMorph *self, void *raw, short in_size,
    char *morph_out, int * morph_out_size, bool robot);

/**************************************************************************
 *Function  -   release morph instance
 *
 *Input		-	None
 *Output	-	None
 *
 **************************************************************************/
void VoiceMorph_Release(VoiceMorph **self);

#endif
