#ifndef _PITCH_CORE_H_
#define _PITCH_CORE_H_

void DedirectAndWindow(float *in, short in_len, float *out, short *out_len);
float FindLocalPitchPeak(float *in, short in_len);
void AutoCorrelation(float *in, float *corr_buf);
void FindPitchCand(float *corr, float *freq_seq, float *intens_seq,
                   short max_delay_pts, short min_delay_pts, float voice_thrd,
                   float intens_thrd, float sampling_rate, short max_cand_num,
                   short *cand_num, float *win_pitch_frame);
float SelectBestPitchCand(float *intens_seq, float *freq_seq, short cand_num,
                          float *last_pitch, short *confidence,
                          short *final_pitch_flag, float freq_min,
                          float freq_max, short pitch_ready,
                          float pitch_average,
						  float *seg_pitch_primary, float *seg_pitch_new, float *pitch_cand_buf,int *un_confidence);
void LongTermPitchEsitmate(float best_cand, short *seg_counting_flag,
                           short *seg_pitch_count,
                           short *pitch_avail_inbuf_count,
                           short *pitch_mean_ready, float *pitch_record,
                           float *pitch_mean, short *pitch_valid_len);
void *GenPitchCandTotalNum(int *data_size);
float Peak_avg_update(float best_cand, short *peak_count, float *peak_record, float local_peak, float *peak_sum);
int zero_pass(float *frame_shift_buf, int len);
#endif
