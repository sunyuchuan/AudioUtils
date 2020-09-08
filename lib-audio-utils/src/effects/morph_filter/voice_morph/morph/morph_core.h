#ifndef _MORPH_CORE_H_
#define _MORPH_CORE_H_
#include "pitch_tracker/src/pitch_macro.h"
float VoiceMorphGetPitchFactor(float pitch_coeff);

void VoiceMorphPitchTransform(float pitch, float ratio, float range_factor,
                              float *pitch_tran);


typedef struct refactortype {
	double startofnoise;
	double endofnoise;
	double startofvoice;
	double endofvoice;
	double ttarget;				//ʵ��ʱ���
	long fake_time_num;			//���ʱ���
	long start_flag;
	long count_peak;
	int last_voice_flag;
	double last_source_pitch_time[10];
	double last_new_pitch[10];
	double last_primary_pitch[10];
	int last_flag;
	int cur_flag;
	double cur_source_pitch_time[10];
	double cur_new_pitch[10];
	double cur_primary_pitch[10];
	double last_tnewperiod;
	double cur_tnewperiod;
	float tmp_res[PITCH_FRAME_SHIFT * 6];
	long total_out_len;
	long calculated_count;
	long total_in_count;
	long real_out_count;
}refactor;
void VoiceMorphPitchStretch(refactor *re_factor, float *in, float *new_pitch, float *primary_pitch,
	short *last_peak_offset, float formant_ratio,
	float sampling_rate,
	short *src_acc_pos, float *out, short *out_len,
	short *intermedia_peak);
/*extern refactor *rebuild_factor;
void init_rebuild();
void creat_rebuild();*/
void link_calculate(refactor *re_factor,int *size,float ratio,int *rsmp_in_len);
#endif
