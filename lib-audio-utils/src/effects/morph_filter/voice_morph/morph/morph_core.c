#include <math.h>
#include <string.h>
#include "morph_core.h"
#include "pitch_tracker/src/pitch_macro.h"
#include "math/junior_func.h"
#include <stdio.h>
#include "log.h"
#define SAMPLE_FREQUENCY 44100.0
#define NUMpi  3.1415926535897932384626433832795028841972
#define Re re_factor->
//refactor *rebuild_factor = NULL;


/*void creat_rebuild()
{
	rebuild_factor = (refactor*)malloc(sizeof(refactor));
}
void init_rebuild()
{
	memset(rebuild_factor, 0x0, sizeof(refactor));
	Re fake_time_num = -1;
}*/
void link_calculate(refactor *re_factor,int *size,float ratio,int *rsmp_in_len )
{
	int tmp = 0;
	long count_out = (re_factor->real_out_count);
	if (count_out < re_factor->calculated_count)
	{
		*size = re_factor->calculated_count - count_out;
	}
	if (re_factor->start_flag < 6)
	{
		tmp = re_factor->start_flag;
	}
	else
	{
		tmp = 6;
	}
	memset(re_factor, 0x0, sizeof(refactor));
	re_factor->start_flag = tmp;
	re_factor->count_peak = tmp;
	Re fake_time_num = -1;
	*rsmp_in_len = 0;
}
double findTmid(double ttarget, double *src_plus_time,int num)
{
	double tmid = 0;
	if (num == 1)
	{
		return src_plus_time[0];
	}
	for (int k = 0; k < num-1; k++)
	{
		if (ttarget < src_plus_time[0])
		{
			tmid = src_plus_time[0];
			break;
		}
		else if (ttarget > src_plus_time[num-1])
		{
			tmid = src_plus_time[num-1];
			break;
		}
		else
		{
			if ((ttarget >= src_plus_time[k]) && (ttarget <= src_plus_time[k + 1]))
			{
				double left_tmp = ttarget - src_plus_time[k];
				double right_tmp = src_plus_time[k + 1] - ttarget;
				if (left_tmp > right_tmp)
				{
					tmid = src_plus_time[k + 1];
					break;
				}
				else
				{
					tmid = src_plus_time[k];
					break;
				}
			}
		}
	}
	return tmid;
}
void copyRise(float *in, double tmin, double tmax, float *out, double tmaxTarget,long fake_time_num, float *tmp_res,short *out_size,float formant_ratio,long *total_out_len) {//�������ݣ�����ʱ��߽磬������ݣ�������ݵĲ���ʱ��
	long imin, imax, imaxTarget, distance, i;
	long source_imin,source_imax;
	double dphase;


	imin = (long)ceil(tmin *SAMPLE_FREQUENCY) + 1;   //计算左时间边界的绝对采样点位置
	if (imin < 1) imin = 1;
	imax = (long)ceil(tmax*SAMPLE_FREQUENCY);   //计算右时间边界的绝对采样点位置

	//imin = imax - (imax - imin)*formant_ratio;


		  /* Not xToLowIndex: ensure separation of subsequent calls. */

	if (imax < imin) return;
	imaxTarget = (long)(ceil(tmaxTarget*SAMPLE_FREQUENCY)*formant_ratio); //输出波峰的绝对采样点位置
	if ((!tmp_res) || (!total_out_len))
	{
		LogInfo("%s tmin %f.\n", __func__, tmin);
		LogInfo("%s tmax %f.\n", __func__, tmax);

		LogInfo("%s imaxTarget %ld.\n", __func__, imaxTarget);
		LogInfo("%s imax %ld.\n", __func__, imax);
		LogInfo("%s imin %ld.\n", __func__, imin);
		LogInfo("%s fake_time_num %ld.\n", __func__, fake_time_num);
		LogInfo("%s formant_ratio %ld.\n", __func__, formant_ratio);
		if (total_out_len)
		{
			LogInfo("%s total_out_len %ld.\n", __func__, *total_out_len);
		}
		return;
	}
	distance = imaxTarget - imax-*total_out_len;							//输出波峰与右时间边界的采样点差值
	dphase = NUMpi / (imax - imin + 1);
	for (i = imin; i <= imax; i++) {
		long iTarget = (i + distance);
		if (iTarget >= 0 && iTarget< PITCH_FRAME_SHIFT*6)
		{
			int tmp = i - fake_time_num * PITCH_FRAME_SHIFT + 2 * PITCH_FRAME_SHIFT;
			if(tmp>=0&&tmp< PITCH_FRAME_SHIFT*9)
			tmp_res[iTarget] += in[i - fake_time_num * PITCH_FRAME_SHIFT + 2 * PITCH_FRAME_SHIFT] * 0.5 * (1 - cos(dphase * (i - imin + 0.5)));	//输出数据
		}
		
	}
	

	while (imaxTarget - *total_out_len > PITCH_FRAME_SHIFT*4)
	{
		*total_out_len += PITCH_FRAME_SHIFT;
		for (int i = 0; i < PITCH_FRAME_SHIFT; i++)
		{
			int tmp = *out_size + i;
			if ((tmp < PITCH_FRAME_SHIFT * 9) && (tmp >= 0))
			{
				out[*out_size + i] = tmp_res[i];
			}
			else
			{
				LogInfo("%s outsize out of range %ld.\n", __func__, *out_size);
				LogInfo("%s tmin %f.\n", __func__, tmin);
				LogInfo("%s tmax %f.\n", __func__, tmax);

				LogInfo("%s imaxTarget %ld.\n", __func__, imaxTarget);
				LogInfo("%s imax %ld.\n", __func__, imax);
				LogInfo("%s imin %ld.\n", __func__, imin);
				LogInfo("%s fake_time_num %ld.\n", __func__, fake_time_num);
				LogInfo("%s formant_ratio %ld.\n", __func__, formant_ratio);
			}

		}
		*out_size += PITCH_FRAME_SHIFT;
		for (int i = 0; i < PITCH_FRAME_SHIFT*5-1; i++)
		{
			tmp_res[i] = tmp_res[i + PITCH_FRAME_SHIFT];
			tmp_res[i + PITCH_FRAME_SHIFT] = 0;
		}
	}
}
void copyFall(float *in, double tmin, double tmax, float *out, double tminTarget,long fake_time_num,float *tmp_res,float formant_ratio, long *total_out_len) {//输入数据，左、右时间边界，输出数据，输出数据的波峰时间
	long imin, imax, iminTarget, distance, i;
	double dphase;
	if (!tmp_res) return;
	if (!total_out_len) return;
	imin = (long)ceil(tmin *SAMPLE_FREQUENCY) + 1;  //Sampled_xToHighIndex (me, tmin);
	if (imin < 1) imin = 1;
	imax = (long)ceil(tmax*SAMPLE_FREQUENCY);  //Sampled_xToHighIndex (me, tmax) - 1;   
	  /* Not xToLowIndex: ensure separation of subsequent calls. */

	//imax = imin + (imax - imin)*formant_ratio;
	if (imax < imin) return;
	iminTarget = (long)(ceil(tminTarget*SAMPLE_FREQUENCY)*formant_ratio) + 1;//Sampled_xToHighIndex (thee, tminTarget);
	if ((!tmp_res) || (!total_out_len))
	{
		LogInfo("%s iminTarget %ld.\n", __func__, iminTarget);
		LogInfo("%s imax %ld.\n", __func__, imax);
		LogInfo("%s imin %ld.\n", __func__, imin);
		LogInfo("%s fake_time_num %ld.\n", __func__, fake_time_num);
		LogInfo("%s formant_ratio %ld.\n", __func__, formant_ratio);
		if (total_out_len)
		{
			LogInfo("%s total_out_len %ld.\n", __func__, *total_out_len);
		}
		return;
	}
	distance = iminTarget - imin-*total_out_len;
	dphase = NUMpi / (imax - imin + 1);
	for (i = imin; i <= imax; i++) {
		long iTarget = i + distance;
		if (iTarget > 0 && iTarget < PITCH_FRAME_SHIFT * 6)
		{
			int tmp = i - fake_time_num * PITCH_FRAME_SHIFT + 2 * PITCH_FRAME_SHIFT;
			if (tmp >= 0 && tmp < PITCH_FRAME_SHIFT * 9)
			tmp_res[iTarget] += in[i - fake_time_num * PITCH_FRAME_SHIFT + 2 * PITCH_FRAME_SHIFT] * 0.5 * (1 + cos(dphase * (i - imin + 0.5)));
		}
		else if (iTarget >= PITCH_FRAME_SHIFT * 6)
		{
			LogInfo("%s iTarget out of range %d.\n", __func__, iTarget);
			LogInfo("%s iminTarget %ld.\n", __func__, iminTarget);
			LogInfo("%s imax %ld.\n", __func__, imax);
			LogInfo("%s imin %ld.\n", __func__, imin);
			LogInfo("%s fake_time_num %ld.\n", __func__, fake_time_num);
			LogInfo("%s formant_ratio %ld.\n", __func__, formant_ratio);
		}
	}

}
void copyBell(refactor *re_factor,float *in, double tmid, double leftWidth, double rightWidth, float *out, double tmidTarget,long fake_time_num,short *out_size,float formant_ratio) {

	if ((!re_factor) || (!in) || (!out) || (!out_size))
	{
		LogInfo("%s tmid %f.\n", __func__, tmid);
		LogInfo("%s leftWidth %f.\n", __func__, leftWidth);
		LogInfo("%s rightWidth %f.\n", __func__, rightWidth);
		LogInfo("%s tmidTarget %f.\n", __func__, tmidTarget);
		LogInfo("%s fake_time_num %ld.\n", __func__, fake_time_num);
		return;
	}
	copyRise(in, tmid - leftWidth, tmid, out, tmidTarget, fake_time_num, Re tmp_res, out_size, formant_ratio,&Re total_out_len);
	copyFall(in, tmid, tmid + rightWidth, out, tmidTarget, fake_time_num, Re tmp_res, formant_ratio,&Re total_out_len);
}



float VoiceMorphGetPitchFactor(float pitch_coeff) {
    if (pitch_coeff > 1.2f) {
        return 1.3f;
    } else if (pitch_coeff > 1.1f) {
        return 1.2f;
    } else if (pitch_coeff > 1.0f) {
        return 1.1f;
    } else if (pitch_coeff >= 0.9f) {
		if (pitch_coeff == 1.0f)
			return 1.0f;
        return 0.9f;
    } else if (pitch_coeff >= 0.8f) {
        return 0.8f;
    } else {
        return 0.7f;
    }
}

void VoiceMorphPitchTransform(float pitch, float ratio, float range_factor,
                              float *pitch_tran) {
    if (pitch > 0.0f) {
        float rised_pitch = pitch * ratio;
        float tmp1 = 4.405e-014f * rised_pitch - 1.215e-010f;
        float tmp2 = tmp1 * rised_pitch + 1.299e-007f;
        tmp1 = tmp2 * rised_pitch - 6.907e-005f;
        tmp2 = tmp1 * rised_pitch + 0.02078f;
        float log_pitch = tmp2 * rised_pitch + 3.064f;
        tmp1 = log_pitch - 4.7874917f;
        tmp1 = 3.15641287f + range_factor * 17.31234f * tmp1;
        float f = tmp1 * 0.057762265f;
        tmp1 = 0.1017f * f + 0.1317f;
        tmp1 = tmp1 * f + 0.4056f;
        tmp1 = tmp1 * f + 1.042f;
        *pitch_tran = 100.0f * (tmp1 * f + 1.021f);
    } else {
        *pitch_tran = 0.0f;
    }
}


float SegFreq(short pos, float *freq_vec, short shift) {
    short seg = (pos + 1) / shift - 2;  // the first two segments are affiliate
    if (seg < 0) seg = 0;
    return freq_vec[seg];
}

short NextPitchPeak(float *data, short peak_pos, short period, short low_bound,
                    short high_bound, short *pitch_peak) {
    short start = peak_pos + low_bound - 1;
    short stop = min(peak_pos + high_bound - 1, MORPH_BUF_LEN);
    short pitch_peak_count = 0;

    if (peak_pos >= 0) {		//找到接下来的一段时间内的波峰（波谷）
        for (short i = start; i < stop; i++) {
			if (i >= 0 && i < PITCH_FRAME_SHIFT * 9)
			{
				if ((data[i] >= 0.05f && data[i] >= data[i - 1] &&
					data[i] >= data[i + 1]) ||
					(data[i] < -0.05f && data[i] <= data[i - 1] &&
						data[i] <= data[i + 1])) {
					if (pitch_peak_count < PITCH_FRAME_SHIFT)
					{
						pitch_peak[pitch_peak_count] = i;
						pitch_peak_count++;
					}
				}
			}
        }
    }

    if (pitch_peak_count == 0) {
        if (fabs(data[start]) >= fabs(data[stop]))
            pitch_peak[0] = start;
        else
            pitch_peak[0] = stop;
        return pitch_peak[0];
    }

    float max_val = -1e10f;
    short index = -1;
    short half_period = period >> 1;
    short left_len1 = peak_pos - max(peak_pos - half_period, 0);
    short right_len1 = min(peak_pos + half_period, MORPH_BUF_LEN) - peak_pos;
    for (short q = 0; q < pitch_peak_count; q++) {		//寻找最近一周期内的互相关函数最大的波峰，
        short left_len2 = pitch_peak[q] - max(pitch_peak[q] - half_period, 0);
        short right_len2 =
            min(pitch_peak[q] + half_period, MORPH_BUF_LEN) - pitch_peak[q];
        short left_len = min(left_len1, left_len2);
        short right_len = min(right_len1, right_len2);
        float *vec1 = &data[peak_pos - left_len - 1];
        float *vec2 = &data[pitch_peak[q] - left_len - 1];
        float cross_term = 0.0f;
        float norm1 = 0.0f;
        float norm2 = 0.0f;
        for (short k = 0; k < (left_len + right_len + 1); k++) {
            cross_term += *vec1 * *vec2;
            norm1 += *vec1 * *vec1;
            norm2 += *vec2 * *vec2;
            vec1++;
            vec2++;
        }
        float corr_val = _reciprocal_sqrt(norm1 * norm2 + 1e-20f) * cross_term;
        if (corr_val > max_val) {
            max_val = corr_val;
            index = q;
        }
    }

    return pitch_peak[index];
}


short FindLocalPeak(float *buf, short start, short end) {
    float max_val = -1e10f, min_val = 1e10f;
    short pos_max = 0, pos_min = 0;
    for (short i = start; i <= end; i++) {
        if (buf[i] > max_val) {
            max_val = buf[i];
            pos_max = i;
        }
        if (buf[i] < min_val) {
            min_val = buf[i];
            pos_min = i;
        }
    }

    if (fabs(max_val) < fabs(min_val))
        return (pos_max + 1);
    else
        return (pos_min + 1);
}




//     affiliate               core segment                 affiliate
// /---------------/------------------------------/---------------------
// |_______|_______|_______|_______|_______|_______|_______|_______|_______|
//                     |                                               |
//                     |                                               |
//                    seg1                                            seg7

void VoiceMorphPitchStretch(refactor *re_factor,float *in, float *new_pitch, float *primary_pitch,
                            short *last_peak_offset, float formant_ratio,	
                            float sampling_rate,
                            short *src_acc_pos, float *out, short *out_len,
                            short *intermedia_peak) {
	
	double voicelessPeriod = 0.003990929705;
	float tperiod = 0;
	double tmid = 0;
	double tmid_test = 0;


	if (Re start_flag < 6)
	{
		*last_peak_offset += PITCH_FRAME_SHIFT;
		Re start_flag++;
		Re count_peak++;
		Re ttarget = 0.5*voicelessPeriod;
	}
	else
	{
		Re calculated_count += PITCH_FRAME_SHIFT;
		
		Re count_peak++;
		for (int i = 0; i < Re cur_flag; i++)						//先更新波峰信息
		{
			Re last_new_pitch[i] = Re cur_new_pitch[i];
			Re last_primary_pitch[i] = Re cur_primary_pitch[i];
			Re last_source_pitch_time[i] = Re cur_source_pitch_time[i];
			Re cur_new_pitch[i] = 0;
			Re cur_primary_pitch[i] = 0;
			Re cur_source_pitch_time[i] = 0;
		}
		Re last_flag = Re cur_flag;
		Re cur_flag = 0;
		Re last_tnewperiod = Re cur_tnewperiod;

		/*更新波峰位置*/
		Re fake_time_num++;
		if (new_pitch[1] == 0.0f)	//如果当前帧的基频为0，暂时不做处理
		{
			*last_peak_offset += PITCH_FRAME_SHIFT;
		}
		else						//如果当前帧的基频不为0，则开始计算波峰位置，将波峰位置存储在peak_time序列中
		{
			float period = 0;
			Re cur_tnewperiod = _reciprocal_sqrt(new_pitch[1] * new_pitch[1]);
			period =
				floor(_reciprocal_sqrt(primary_pitch[1] * primary_pitch[1]) *
					sampling_rate);
			if (new_pitch[0] == 0.0f&&new_pitch[1] != 0.0f)	//如果从噪声进入语音段，则需要重新定位波峰
			{
				*last_peak_offset = FindLocalPeak(in, (*last_peak_offset), (*last_peak_offset) + 1.2*period);
				*last_peak_offset -= period;
			}
			//计算原基频的周期占用的点数
			short search_pos = *last_peak_offset;				//寻找的中心点选取为波峰，该波峰为上一次向后寻找到的波峰
			short loop_region = -1;
			short peak_count = 0;
			short ret_peak;
			float freq_vec[12] = { 0.0f };
			short peak_vec[12] = { 0 };
			double timechange = 0;
			if (*last_peak_offset <= IN_RANGE_LEN) {			//寻找当前点之后的0.8到1.25个周期内的互相关最大的波峰
				short search_low = (short)(0.8f * period);  // should be 0.83333f
				short search_high = (short)(1.25f * period);
				if ((search_low + *last_peak_offset) <= IN_RANGE_LEN) {
					while (loop_region <= IN_RANGE_LEN&& peak_count<10) {
						ret_peak =
							NextPitchPeak(in, search_pos, (short)period, search_low,
								search_high, intermedia_peak);
						peak_vec[peak_count] = ret_peak;
						loop_region = ret_peak;
						search_pos = ret_peak;
						freq_vec[peak_count] =
							SegFreq(ret_peak, new_pitch, PITCH_FRAME_SHIFT);
						peak_count++;
					}
				}
				else {
					ret_peak =
						NextPitchPeak(in, search_pos, (short)period, search_low,
							search_high, intermedia_peak);
					peak_vec[peak_count] = ret_peak;
					freq_vec[peak_count] =
						SegFreq(ret_peak, new_pitch, PITCH_FRAME_SHIFT);
					peak_count++;
				}
			}

			if (peak_count == 0) {
			}
			else if (peak_count >= 1) {
				for (int i = 0; i < peak_count; i++)
				{
					if (i < 10)
					{
						Re cur_flag++;
						long tmp = peak_vec[i] + (Re count_peak) * PITCH_FRAME_SHIFT - MORPH_BUF_LEN;
						Re cur_source_pitch_time[i] = ((double)tmp) / SAMPLE_FREQUENCY;
						Re cur_primary_pitch[i] = 1 / primary_pitch[1];
						Re cur_new_pitch[i] = 1 / new_pitch[1];
					}
					else
					{
						LogInfo("%s peak_count out of range %d.\n", __func__, peak_count);
					}
				}
				*last_peak_offset = peak_vec[peak_count - 1];
			}
		}

		if (Re last_voice_flag)			//如果判定这一帧开始是语音的情况
		{
			if (Re cur_flag)			//如果下一帧有波峰，那么在下一帧的波峰处进行判断
			{
				int i = 0;
				for ( i = 0; i < Re last_flag; i++)	//如果在帧的中间找到了
				{
					if (i < 9)
					{
						if (Re last_source_pitch_time[i + 1] - Re last_source_pitch_time[i] > 0.012)
						{
							Re endofvoice = Re last_source_pitch_time[i] + 0.5*Re last_primary_pitch[i];
							break;
						}
					}
					else
					{
						LogInfo("%s last_flag out of range %d.\n", __func__, Re last_flag);
					}
					
				}
				if (i == Re last_flag)	//如果没找到间隔大于阈值的，就把最后一个点做为语音结束位置,并指定下一帧还是语音信号
				{
					Re endofvoice = Re last_source_pitch_time[i-1];
					while (Re ttarget < Re endofvoice)
					{
						tmid = findTmid(Re ttarget, Re last_source_pitch_time, Re last_flag);
						/*if (1 / primary_pitch[0] > Re last_tnewperiod)
						{
							copyBell(in, tmid, 1 / primary_pitch[0], 1 / primary_pitch[0], out, Re ttarget, Re fake_time_num, out_len, formant_ratio);
						}
						else {
							//copyBell(in, tmid, Re last_tnewperiod, Re last_tnewperiod, out, Re ttarget, Re fake_time_num, out_len, formant_ratio);
							copyBell(in, tmid, 1 / primary_pitch[0], 1 / primary_pitch[0], out, Re ttarget, Re fake_time_num, out_len, formant_ratio);
						}*/
						copyBell(re_factor,in, tmid, 1 / primary_pitch[0], 1 / primary_pitch[0], out, Re ttarget, Re fake_time_num, out_len, formant_ratio);
						Re ttarget += (Re last_tnewperiod)/ formant_ratio;
						//Re ttarget += (Re last_tnewperiod);
					}
					Re last_voice_flag = 1;
				}

			}
			else					//如果下一帧没有波峰，那么就判断下一帧为噪声
			{
				Re endofvoice = Re last_source_pitch_time[Re last_flag - 1] + 0.5*Re last_primary_pitch[Re last_flag - 1];
				Re startofnoise = Re endofvoice;
				while (Re ttarget < Re endofvoice)
				{
					tmid = findTmid(Re ttarget, Re last_source_pitch_time, Re last_flag);
					/*if (1 / primary_pitch[0] > Re last_tnewperiod)
					{
						copyBell(in, tmid, 1 / primary_pitch[0], 1 / primary_pitch[0], out, Re ttarget, Re fake_time_num, out_len, formant_ratio);
					}
					else
					{
						copyBell(in, tmid, Re last_tnewperiod, Re last_tnewperiod, out, Re ttarget, Re fake_time_num, out_len, formant_ratio);
					}*/
					copyBell(re_factor,in, tmid, 1 / primary_pitch[0], 1 / primary_pitch[0], out, Re ttarget, Re fake_time_num, out_len, formant_ratio);
					Re ttarget += Re last_tnewperiod / formant_ratio;
					//Re ttarget += Re last_tnewperiod;
					//float tmp_test = rebuild_factor->ttarget;
				}
				Re ttarget = Re last_source_pitch_time[Re last_flag - 1] + 0.5*Re last_primary_pitch[Re last_flag - 1] - 0.5*voicelessPeriod;
				Re last_voice_flag = 0;
				Re endofnoise = Re startofnoise;
			}
		}
		else			//如果判断这一帧开始是噪声,那么last肯定是噪声，直接判断cur是不是有波峰
		{
			if (Re cur_flag)	//如果cur有波峰，那就把噪声结束的时间位置定义到第一个波峰位置-半个周期
			{
				Re endofnoise = Re cur_source_pitch_time[0] - 0.5*Re cur_new_pitch[0];
				while (Re ttarget < Re endofnoise)
				{
					copyBell(re_factor,in, Re ttarget, voicelessPeriod, voicelessPeriod, out, Re ttarget, Re fake_time_num, out_len,formant_ratio);
					Re ttarget += voicelessPeriod;
				}
				Re ttarget = Re cur_source_pitch_time[0];
				Re last_voice_flag = 1;
			}
			else		//如果cur没有波峰，则把噪声结束时间加一帧
			{
				Re endofnoise += PITCH_FRAME_SHIFT / SAMPLE_FREQUENCY;
				while (Re ttarget < Re endofnoise)
				{
					copyBell(re_factor,in, Re ttarget, voicelessPeriod, voicelessPeriod, out, Re ttarget, Re fake_time_num, out_len,formant_ratio);
					Re ttarget += voicelessPeriod / formant_ratio;
					//Re ttarget += voicelessPeriod;
				}
				Re last_voice_flag = 0;
			}
		}
	}

}
