#ifndef _PITCH_TRACKER_H_
#define _PITCH_TRACKER_H_

#include <stdio.h>

typedef struct PitchTracker {
    short shift_buf_pos;
    float min_freq;
    float max_freq;
    short min_lag;
    short max_lag;
    float sample_rate;
    float voice_thrd;
    float intens_thrd;
    short cand_num;
    short pitch_cand_buf_len;
    float local_peak_thrd;
    float prev_pitch;
    short successive_confidence;
    short last_pitch_flag;
    float long_term_pitch;
    short long_term_pitch_count;
    short long_term_pitch_ready;
    short pitch_counting_flag;
    short successive_pitch_seg_count;
    short pitch_inbuf_count;
    float *pitch_buf;
    float *frame_shift_buf;
    float *pitch_cand_buf;
    float *long_term_pitch_record;
    float *autocorr_buf;
    float *freq_cand_seq;
    float *intensity_cand_seq;
    float *win_pitch_frame;

    float peak_record[10];
    float peak_sum;
    short peak_count;
    float peak_avg;
	int mute_count;
	int un_confidence;
} PitchTracker;

PitchTracker* PitchTracker_Create();
int PitchTracker_Init(PitchTracker* self,
    float freq_upper_bound, float freq_lower_bound,
    float voice_max_power, float intens_min_power,
    short candidate_max_num, float frame_peak_thrd);
int PitchTracker_Process(PitchTracker* self,
    short *in, short in_len, float *float_buf,
    float *seg_pitch_primary, float *seg_pitch_new);
void PitchTracker_Release(PitchTracker** pTracker);

#endif
