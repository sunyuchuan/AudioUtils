#include "pitch_core.h"
#include "math/junior_func.h"
#include <math.h>
#include "pitch_macro.h"

int pitch_cand_send_buf[PITCH_MINIMUM_SEND_THRD * (MAX_CAND_NUM * 2 + 2) + 1] =
    {0};
int total_cand_num = 0;

void *GenPitchCandTotalNum(int *data_size) {
    pitch_cand_send_buf[0] = total_cand_num;
    *data_size = sizeof(int);
    return (void *)pitch_cand_send_buf;
}

void DedirectAndWindow(float *in, short in_len, float *out, short *out_len) {
    short i, j;
    float mean, sum1 = 0, sum2 = 0, sum3 = 0, sum4 = 0;

    for (i = 0; i < PITCH_BUFFER_LENGTH; i += 4) {
        sum1 += in[i];
        sum2 += in[i + 1];
        sum3 += in[i + 2];
        sum4 += in[i + 3];
    }
    mean = (sum1 + sum2 + sum3 + sum4) * INV_PITCH_BUFFER_LENGTH;

    for (i = 0, j = PITCH_BUFFER_LENGTH - 1; i < (PITCH_BUFFER_LENGTH >> 1);
         i++, j--) {
        out[i] = in[i] - mean;
        out[j] = in[j] - mean;
    }
    *out_len = in_len;
}
int zero_pass(float *frame_shift_buf,int len)
{
	int count = 0;
	for (int i = 1; i < len - 1; i++)
	{
		if (frame_shift_buf[i - 1] * frame_shift_buf[i + 1] < 0)
		{
			count++;
		}
	}
	return count;
}
float FindLocalPitchPeak(float *in, short in_len) {
    short i;
    float max_val = 0.0f, tmp;
    for (i = 0; i < in_len; i++) {
        tmp = (in[i] < 0.0f) ? (-in[i]) : in[i];
        if (tmp > max_val) {
            max_val = tmp;
        }
    }

    return max_val;
}

void AutoCorrelation(float *in, float *corr_buf) {
    short i, j;
    float *corr, *x, tmp32no1 = 0.0f, tmp32no2 = 0.0f, tmp32no3 = 0.0f,
                     tmp32no4 = 0.0f, norm2, inv_norm;
    x = in;
    corr = corr_buf;
	int count = 0;
    /*for (i = 0; i < PITCH_BUFFER_LENGTH; i += 8) {		//�൱��4���²������������غ���
        tmp32no1 += (x[i] * x[i]);
        tmp32no2 += (x[i + 4] * x[i + 4]);
    }

	norm2 = tmp32no1 +tmp32no2;*/
	/*�޸���ʹ֡�������Ƽ�С*/
	for (i = 0; i < PITCH_BUFFER_LENGTH; i += 4) {		//�൱��4���²������������غ���
		tmp32no1 += (x[i] * x[i]);
		count++;
	}
	norm2 = tmp32no1/count;
	/*�޸���ʹ֡�������Ƽ�С*/

    inv_norm = _reciprocal_sqrt(norm2 * norm2);	//norm2��ƽ�����ĵ������������������������

    for (i = 0; i < PITCH_BUFFER_LENGTH / 4; i++) {		//4���²���������������㣬�õ��Ľ������corr��
        tmp32no1 = 0;
		count = 0;
        for (j = 0; j < (PITCH_BUFFER_LENGTH - 4 * i); j += 4) {
            tmp32no1 += x[j] * x[4 * i + j];  // q20
			count++;
        }
        corr[i] = tmp32no1 * inv_norm/count;
    }
	
    corr[0] = 1.0f;
}

void FindPitchCand(float *corr, float *freq_seq, float *intens_seq,
                   short max_delay_pts, short min_delay_pts, float voice_thrd,
                   float intens_thrd, float sampling_rate, short max_cand_num,
                   short *cand_len,float *win_pitch_frame) {
    short cand_index = 0;
    float *xc = corr, half_voice_thrd = 0.1f * voice_thrd, dr, d2r, freq_est,
          tmp1, tmp2;
    short start = min_delay_pts / 4 - 2;
    short end = max_delay_pts / 4 - 1;
	float test_thrd = 0.36;
	float yin_source[10] = { 0 };
    for (short i = start; i < end; i++) {
        if (xc[i] > test_thrd && xc[i] > xc[i - 1] &&
            xc[i] > xc[i + 1])  // may be a candidate
        {
            dr = 0.5f * (xc[i + 1] - xc[i - 1]);
            d2r = 2.0f * xc[i] - xc[i + 1] - xc[i - 1];			//sound_to_pitch.cpp 395
            tmp1 = ((float)i) * d2r + dr;
            tmp2 = _reciprocal_sqrt(tmp1 * tmp1);
            freq_est = tmp2 * d2r * sampling_rate * 0.25f;
            if (xc[i] > intens_thrd && cand_index < max_cand_num) {
                intens_seq[cand_index] = xc[i];
                freq_seq[cand_index] = freq_est;
                cand_index++;
            }
        }
    }
	/*�жϰ�Ƶ����Ƿ����*/
	if (cand_index >= 2)
	{
		float max_intens = 0;
		short peak_pos_record[10] = { 0 };
		short peak_count = 0;
		short peak_distance_sum = 0;
		short peak_distance_avg = 0;
		//for (int i = 1; i < PITCH_BUFFER_LENGTH - 1; i++)
		for (int i = 1; i < PITCH_FRAME_SHIFT - 1; i++)
		{
			if (win_pitch_frame[i] > win_pitch_frame[i - 1] && win_pitch_frame[i] > win_pitch_frame[i + 1])
			{
				peak_pos_record[peak_count++] = i;
				if (peak_count > 9)
					break;
			}	
		}
		

		for (int i = 1; i < peak_count; i++)
		{
			peak_distance_sum += peak_pos_record[i] - peak_pos_record[i - 1];
			if (max_intens < intens_seq[i])
			{
				max_intens = intens_seq[i];
			}
		}
		if (peak_count > 1)
		{
			peak_distance_avg = LOCAL_SAMPLE_RATE_FLOAT / (peak_distance_sum / (peak_count - 1));
		}
		else
		{
			peak_distance_avg = 0;
		}

		for (int i = 0; i < cand_index; i++)
		{
			if (peak_distance_avg > freq_seq[i] * 0.8&&peak_distance_avg < freq_seq[i] * 1.3&&max_intens - intens_seq[i] < 0.1)
			{
				freq_seq[0] = freq_seq[i];
				intens_seq[0] = intens_seq[i];
				cand_index = 1;
			}
		}
		
	}
	/*yin�ķ�����ѡȡ��Ƶ*/
	/*if (cand_index >= 2)
	{
		float max_intens = 0;
		int cand_count = cand_index;
		float max_peak = 0;
		short max_peak_pos = 0;
		short peak_count = 0;
		short peak_distance_sum = 0;
		short peak_distance_avg = 0;
		for (int i = PITCH_FRAME_SHIFT; i < PITCH_BUFFER_LENGTH - PITCH_FRAME_SHIFT; i++)		//�ҵ����Ĳ���λ��
		{
			if (win_pitch_frame[i] > win_pitch_frame[i - 1] && win_pitch_frame[i] > win_pitch_frame[i + 1])
			{
				if (max_peak < win_pitch_frame[i])
				{
					max_peak = win_pitch_frame[i];
					max_peak_pos = i;
				}
			}
		}

		for (int i = 0; i < cand_index; i++)
		{
			float fre_cand = freq_seq[i];
			float phase = 2*PI / fre_cand;
			float amplitude = max_peak;
			short num = (short)(LOCAL_SAMPLE_RATE_FLOAT / fre_cand);

			if (max_peak_pos + num < PITCH_BUFFER_LENGTH*1.5)			//�ж��ұߵ����г����Ƿ��㹻���㷨��������������������
			{
				for (int j = 0; j < num; j++)
				{
					yin_source[i] += fabs(win_pitch_frame[j + max_peak_pos] - amplitude*cos(j*phase));
				}
			}
			else {
				for (int j = 0; j < num; j++)
				{

					yin_source[i] += fabs(win_pitch_frame[max_peak_pos-j] - amplitude * cos(j*phase));
				}
			}
			yin_source[i] = yin_source[i] / num;
		}

		float min_source = yin_source[0];
		short min_source_pos = 0;
		for (int i = 1; i < cand_index; i++)
		{
			if (min_source > yin_source[i])
			{
				min_source_pos = i;
			}
		}
		cand_index = 1;
		intens_seq[0] = intens_seq[min_source_pos];
		freq_seq[0] = freq_seq[min_source_pos];
	}*/

    *cand_len = cand_index;
}




float SelectPitch_Cand1_ConfGtThrd(short pitch_ready, float freq_min,
                                   float freq_max, float *freq_seq,
                                   float last_pitch,int *un_confidence) {
    float inv, mul;
    if (pitch_ready == 0) {										//�����ʷ�ϵĻ�Ƶ��ֵû�м���ã���ֱ�ӷ���Ψһ�ĺ�ѡ��Ƶ
        return freq_seq[0];
    } else {
        if (freq_seq[0] >= freq_min && freq_seq[0] <= freq_max) {	//���Ψһ�ĺ�ѡ��Ƶ�ں��ʷ�Χ�ڣ��ͷ��ظû�Ƶֵ
			*un_confidence = 0;
			return freq_seq[0];
        } else if (freq_seq[0] > 1.75f * last_pitch) {
           /* inv = _reciprocal_sqrt(last_pitch * last_pitch);
            mul = inv * freq_seq[0];
            mul = round_float(mul);
            inv = _reciprocal_sqrt(mul * mul);*/
			if (*un_confidence > 0)
			{
				*un_confidence = 0;
				return freq_seq[0];
			}
			else
			{
				(*un_confidence)++;
				return last_pitch;
			}
            //return freq_seq[0] * inv;
        } else if (freq_seq[0] > freq_max &&
                   freq_seq[0] <= 1.75f * last_pitch) {
            return freq_max;
        } else if (freq_seq[0] < freq_min && freq_seq[0] < 0.7f * last_pitch) {	//�����һ�μ�������Ļ�ƵС�ڻ�Ƶ��ֵ
           /* inv = _reciprocal_sqrt(freq_seq[0] * freq_seq[0]);		//��С����һ�λ�Ƶ��0.7��������Ϊ��һ�εĻ�Ƶ�а�Ƶ���߸�С��ƫ��
            mul = inv * last_pitch;									//ʵ�ʵĻ�Ƶ�Ǻ�ѡ��Ƶ������������
            mul = round_float(mul);*/
			if (*un_confidence > 0)
			{
				*un_confidence = 0;
				return freq_seq[0];
			}
			else
			{
				(*un_confidence)++;
				return last_pitch;
			}
            //return freq_seq[0] * mul;
        } else if (freq_seq[0] < freq_min && freq_seq[0] >= 0.7f * last_pitch) {
            return freq_min;
        }
    }
    return 0.0f;
}

float SelectPitch_Cand1_ConfLtThrd(short pitch_ready, float pitch_mean,
                                   float fraction, float *freq_seq) {
    float inv, mul;
    float upper_bound = (1.0f + fraction) * pitch_mean,
          lower_bound = (1.0f - fraction) * pitch_mean;

    if (pitch_ready == 0) {
        return freq_seq[0];
    } else {
        if (freq_seq[0] <= upper_bound && freq_seq[0] >= lower_bound) {
            return freq_seq[0];
        } else if (freq_seq[0] > upper_bound) {
            inv = _reciprocal_sqrt(pitch_mean * pitch_mean);
            mul = freq_seq[0] * inv;
            mul = round_float(mul);
            inv = _reciprocal_sqrt(mul * mul);
            return freq_seq[0] * inv;
        } else if (freq_seq[0] < lower_bound) {
            inv = _reciprocal_sqrt(freq_seq[0] * freq_seq[0]);
            mul = pitch_mean * inv;
            mul = round_float(mul);
            return freq_seq[0] * mul;
        }
    }
    return 0.0f;
}

float SelectPitch_CandGt1_ConfGtThrd(short pitch_ready, float freq_min,
                                     float freq_max, float *freq_seq,
                                     float *inens_seq, short cand_num,
                                     float pitch_average, float prev_pitch,
                                     short pos) {
    float inv, max_val = 0.0f, delta1, delta2, part1, part2, score;
    short possible_pos = -1, i;

    if (pitch_ready == 0) {
        for (i = 0; i < cand_num; i++) {
            delta1 = fabs(freq_seq[i] - prev_pitch);
            if (delta1 > prev_pitch) {					//part1��Ӧ�˵�ǰ֡����һ֡�Ļ�Ƶ�����Ƴ̶�
                part1 = 0.0f;
            } else {
                inv = _reciprocal_sqrt(prev_pitch * prev_pitch);
                part1 = 1.0f - inv * delta1;
            }
            score = 0.5f * part1 + 0.5f * inens_seq[i];	//ÿ����ѡ��Ƶ�ĵ÷֣��÷������Ƴ̶Ⱥͺ�ѡ��Ƶ������س̶ȼ�Ȩ�õ�
            if (score > max_val) {
                max_val = score;
                possible_pos = i;
            }
        }
        return freq_seq[possible_pos];			//���ص÷������Ǹ���Ƶ
    } else {
        for (i = 0; i < cand_num; i++) {
            delta1 = fabs(freq_seq[i] - prev_pitch);
            if (delta1 > prev_pitch) {
                part1 = 0.0f;
            } else {
                inv = _reciprocal_sqrt(prev_pitch * prev_pitch);
                part1 = 1.0f - inv * delta1;
            }

            delta2 = fabs(freq_seq[i] - pitch_average);
            if (delta2 > prev_pitch) {
                part2 = 0.0f;
            } else {
                inv = _reciprocal_sqrt(pitch_average * pitch_average);
                part2 = 1.0f - inv * delta2;
            }
            score = 0.1f * part1 + 0.1f * part2 + 0.8f * inens_seq[i];
            if (score > max_val) {
                max_val = score;
                possible_pos = i;
            }
        }
        return freq_seq[possible_pos];
    }
    return 0.0f;
}

float SelectPitch_CandGt1_ConfLtThrd(short pitch_ready, float *freq_seq,
                                     float *intens_seq, short cand_num,
                                     float pitch_average, float last_pitch,
                                     short pos) {
    float max_val = 0.0f, delta1, part1, delta2, part2, inv, score;
    short possible_pos = -1, i;

    if (pitch_ready == 0) {
        return freq_seq[pos];
    } else {
        for (i = 0; i < cand_num; i++) {
            inv = _reciprocal_sqrt(pitch_average * pitch_average);
            delta1 = fabs(pitch_average - freq_seq[i]);
            if (delta1 < pitch_average) {
                part1 = 1.0f - delta1 * inv;
            } else {
                part1 = 0.0f;
            }

            delta2 = fabs(last_pitch - freq_seq[i]);
            inv = _reciprocal_sqrt(last_pitch * last_pitch);
            if (delta2 < last_pitch) {
                part2 = 1.0f - delta2 * inv;
            } else {
                part2 = 0.0f;
            }
            score = part1 * 0.2f + part2 * 0.2f + intens_seq[i] * 0.6f;
            if (score > max_val) {
                max_val = score;
                possible_pos = i;
            }
        }
        return freq_seq[possible_pos];
    }
    return 0.0f;
}

float SelectPitch_CandGt1_LastPitch0(short pitch_ready, float *freq_seq,
                                     float *intens_seq, short cand_num,
                                     float pitch_average, short pos) {
    float max_val = 0.0f, inv, div_val, ratio, score;
    short i, possible_pos = -1;

    if (pitch_ready == 0) {
        return freq_seq[pos];
    } else {
        for (i = 0; i < cand_num; i++) {
            inv = _reciprocal_sqrt(pitch_average * pitch_average);	//ƽ����Ƶ���������δ֪
            div_val = inv * freq_seq[i];
            ratio = 1.0f - fabs(1.0f - div_val);				//��ʾ��ѡ��Ƶ��ƽ����Ƶ�����ƶ�
            if (ratio < 0.0f) {
                ratio = 0.0f;
            }
            score = 0.3f * ratio + intens_seq[i] * 0.7f;		//�÷������ƶȺ�����س̶Ⱦ���
            if (score > max_val) {
                max_val = score;
                possible_pos = i;
            }
        }
        return freq_seq[possible_pos];
    }
    return 0.0f;
}

float Peak_avg_update(float best_cand,short *peak_count,float *peak_record,float local_peak,float *peak_sum)
{
	float peak_avg = 0;
	if (best_cand != 0)
	{
		if (*peak_count < 10)
		{
			peak_record[*peak_count] = local_peak;
			(*peak_count)++;
			*peak_sum += local_peak;
		}
		else
		{
			*peak_sum -= peak_record[(*peak_count) % 10];
			peak_record[*peak_count % 10] = local_peak;
			*peak_sum += local_peak;
			peak_avg = *peak_sum / 10;
		}
	}
	else
	{
		peak_avg = *peak_sum / 10;
	}
	return peak_avg;
}








float SelectBestPitchCand(float *intens_seq, float *freq_seq, short cand_num,
                          float *last_pitch, short *confidence,
                          short *final_pitch_flag, float freq_min,
                          float freq_max, short pitch_ready,	//pitch_ready��ʾ��ʷ�ϵ�ƽ����Ƶ�Ƿ�������
                          float pitch_average,
						  float *seg_pitch_primary, float *seg_pitch_new,float *pitch_cand_buf,int *un_confidence) {
    float upper_freq, lower_freq, best_freq = 0.0, max_val;
    short i, pos;
    upper_freq = min(*last_pitch * 1.2f, freq_max);		//�����ϴλ�Ƶ����ʷ�ϵĻ�Ƶƽ��ֵ������õ��������ޣ�
    lower_freq = max(*last_pitch * 0.8f, freq_min);		//������λ�Ƶ�����޺�����

    if (cand_num == 0 || cand_num >= MAX_CAND_NUM) {
        if (*last_pitch != 0.0f && *final_pitch_flag == 0 &&			//�����һ֡�л�Ƶ����һ֡û�л�Ƶ�����������Ŷȴ���4����
            *confidence > PITCH_CONFIDENCE_THRD)						//�ж���һ֡Ϊ���������һ֡����ε����Ż�ƵѡȡΪ��һ֡�����Ż�Ƶ
        {																//��������������־λ��1
            best_freq = *last_pitch;
            *confidence += 1;
            *final_pitch_flag = 1;
        } else if (*last_pitch != 0.0f && *final_pitch_flag == 0 &&		//�����һ֡�л�Ƶ����һ֡û�л�Ƶ�����������ŶȲ�����Ҫ��
                   *confidence <= PITCH_CONFIDENCE_THRD) {				//����Ϊ֮ǰ�Ļ�Ƶ���Ǵ����⣬��εĻ�Ƶ���Ϊ0���������Ŷ�
            best_freq = 0.0f;
            *final_pitch_flag = 0;
			for (int j = 2; j < 5; j++)
			{
				seg_pitch_primary[j] = 0;
				seg_pitch_new[j] = 0;
			}
			pitch_cand_buf[0] = 0;
            *confidence = 0;
        } else if (*last_pitch != 0.0f && *final_pitch_flag == 1) {		//�����һ֡�л�Ƶ�����Ѿ�����עΪ���һ֡����ô��һ֡
            best_freq = 0.0f;											//��ƵΪ0�������Ŷ���Ϊ0����ӳ����ĩ�˵ı�־λ�û�0
            *final_pitch_flag = 0;
            *confidence = 0;
        } else if (*last_pitch == 0.0f) {				//�����һ֡��ƵΪ0����һ֡û�к�ѡ��Ƶ���򷵻���һ֡�Ļ�ƵΪ0���������������Ŷ���Ϊ0.
            best_freq = 0.0f;
            *final_pitch_flag = 0;
            *confidence = 0;
        }
    } else if (cand_num == 1) {
        if (*last_pitch != 0.0f && *confidence > PITCH_CONFIDENCE_THRD) {	//�����һ֡�л�Ƶ�����Ŷ�����Ҫ��
            best_freq = SelectPitch_Cand1_ConfGtThrd(
                pitch_ready, lower_freq, upper_freq, freq_seq, *last_pitch, un_confidence);
            *confidence += 1;
        } else if (*last_pitch != 0.0f &&					//�����һ�εĻ�ƵΪ0����һ�εĻ�Ƶ����ֻ��һ�����������ŶȲ�����Ҫ����ֱ�Ӱ���һ�εĻ�Ƶ���
                   *confidence <= PITCH_CONFIDENCE_THRD) {
            best_freq = freq_seq[0];
            *confidence += 1;
        } else if (*last_pitch == 0.0f) {					//�����һ�εĻ�ƵΪ0����һ�λ�Ƶ����ֻ��һ���������Ltѡ����εĻ�Ƶ���������Ŷ���1
            best_freq = SelectPitch_Cand1_ConfLtThrd(
                pitch_ready, pitch_average, PITCH_VARIANCE_FACTOR, freq_seq);
            *confidence = 1;
        }
        *final_pitch_flag = 0;
    } else if (cand_num < MAX_CAND_NUM) {
        max_val = intens_seq[0];			//Ѱ�һ�Ƶ��ѡ���棬�����ϵ�������Ǹ���Ƶ
        pos = 0;
        for (i = 1; i < cand_num; i++) {
            if (intens_seq[i] > max_val) {
                max_val = intens_seq[i];
                pos = i;
            }
        }
        best_freq = freq_seq[pos];
        if (*last_pitch != 0.0f && *confidence > PITCH_CONFIDENCE_THRD) {	//�����һ֡�л�Ƶ����һ֡�д���һ���Ļ�Ƶ��ѡ
            best_freq = SelectPitch_CandGt1_ConfGtThrd(			//�����������Ŷ��㹻�ߣ����������Ŷ������ѡȡ��һ�ε����Ż�Ƶ
                pitch_ready, lower_freq, upper_freq, freq_seq, intens_seq,
                cand_num, pitch_average, *last_pitch, pos);
            *confidence += 1;
        } else if (*last_pitch != 0.0f &&
                   *confidence <= PITCH_CONFIDENCE_THRD) {	//�����һ֡�л�Ƶ����һ֡�д���һ���Ļ�Ƶ��ѡ�������ŶȲ��ߣ�
            best_freq = SelectPitch_CandGt1_ConfLtThrd(		//���������Ŷ������ѡȡ��ε����Ż�Ƶ�����Ŷ�+1
                pitch_ready, freq_seq, intens_seq, cand_num, pitch_average,
                *last_pitch, pos);
            *confidence += 1;
        } else if (*last_pitch == 0.0f) {					//�����һ֡û�л�Ƶ������һ֡�л�Ƶ�������lastpitchѡȡ��Ƶ 
            best_freq = SelectPitch_CandGt1_LastPitch0(pitch_ready, freq_seq,
                                                       intens_seq, cand_num,
                                                       pitch_average, pos);
            *confidence = 1;
        }
        *final_pitch_flag = 0;
    }
    *last_pitch = best_freq;
    return best_freq;
}

short PitchAverage(float *pitch_mean, short pitch_valid_len,
                   float *pitch_record, short pitch_avail_inbuf_count) {
    short i, invalid_pitch_count = 0;
    float invalid_pitch_sum = 0.0f, pitch_sum = 0.0f, mean_val, inv, tmp,
          upper_mean, lower_mean, valid_count;

    if (*pitch_mean == 0.0f) {			//�����ֵ����0��Ҳ����˵��һ�ν�������ֵ
        for (i = 0; i < pitch_avail_inbuf_count; i++) {
            pitch_sum += pitch_record[i];
        }
        tmp = (float)pitch_avail_inbuf_count;
        inv = _reciprocal_sqrt(tmp * tmp);
        mean_val = inv * pitch_sum;
        upper_mean = 1.75f * mean_val;				//�����������ƽ����ƵΪ���ģ�����������
        lower_mean = 0.25f * mean_val;
        for (i = 0; i < pitch_avail_inbuf_count; i++) {	//Ѱ�һ�Ƶ�в��������޷�Χ�ڵĻ�Ƶ������¼�����ͻ�Ƶ�ܺͣ���һ��
            if (pitch_record[i] > upper_mean || pitch_record[i] < lower_mean) {	//����������ȥ������ĵ㣬ʹ��������ӿɿ�
                invalid_pitch_count += 1;
                invalid_pitch_sum += pitch_record[i];
            }
        }
        valid_count = (float)(pitch_avail_inbuf_count - invalid_pitch_count);
        pitch_sum -= invalid_pitch_sum;
        inv = _reciprocal_sqrt(valid_count * valid_count);
        *pitch_mean = inv * pitch_sum;
    } else {						//������ǵ�һ���������Ǿ�����һ��������������һ������
        upper_mean = 1.35f * *pitch_mean;
        lower_mean = 0.65f * *pitch_mean;
        for (i = 0; i < pitch_avail_inbuf_count; i++) {
            pitch_sum += pitch_record[i];
            if (pitch_record[i] > upper_mean || pitch_record[i] < lower_mean) {
                invalid_pitch_count += 1;
                invalid_pitch_sum += pitch_record[i];
            }
        }
        valid_count = (float)(pitch_valid_len - invalid_pitch_count +			//����ʷ�Ϻ���һ�����������л�Ƶ��ƽ������Ϊ��ǰ�Ļ�Ƶƽ��ֵ
                              pitch_avail_inbuf_count);
        pitch_sum += (pitch_valid_len * *pitch_mean - invalid_pitch_sum);
        inv = _reciprocal_sqrt(valid_count * valid_count);
        *pitch_mean = inv * pitch_sum;
    }
    return (pitch_avail_inbuf_count - invalid_pitch_count);
}

void LongTermPitchEsitmate(float best_cand, short *seg_counting_flag,
                           short *seg_pitch_count,
                           short *pitch_avail_inbuf_count,
                           short *pitch_mean_ready, float *pitch_record,
                           float *pitch_mean, short *pitch_valid_len) {
    short i, valid_increment, index;

    if (best_cand != 0.0f) {
        if (*seg_counting_flag == 0) {
            *seg_counting_flag = 1;
        }

        if (*pitch_avail_inbuf_count >= LONG_TERM_PITCH_REFERENCE_LEN) {
            if (*seg_counting_flag == 1 &&
                *seg_pitch_count <= PITCH_SEG_COUNT_THRD) {
                for (i = (*pitch_avail_inbuf_count - *seg_pitch_count);
                     i < *pitch_avail_inbuf_count; i++) {
                    pitch_record[i] = 0.0f;
                }
                *pitch_avail_inbuf_count -= *seg_pitch_count;
            }
            *seg_pitch_count = 0;
            *seg_counting_flag = 0;
        } else {											//����л�Ƶ�����ʾ������ʼ����¼��Ƶ��
            index = *pitch_avail_inbuf_count;				//������������ʱ�����Ƶ��ƽ��ֵ
            pitch_record[index] = best_cand;
            *seg_pitch_count += 1;
            *pitch_avail_inbuf_count += 1;
        }
    } else {
        if (*seg_counting_flag == 1 &&
            *seg_pitch_count <= PITCH_SEG_COUNT_THRD) {
            for (i = (*pitch_avail_inbuf_count - *seg_pitch_count);
                 i < *pitch_avail_inbuf_count; i++) {
                pitch_record[i] = 0.0f;
            }
            *pitch_avail_inbuf_count -= *seg_pitch_count;
        }
        *seg_pitch_count = 0;
        *seg_counting_flag = 0;
    }

    if (*seg_counting_flag == 0 &&								//�����ǰ֡��ⲻ����Ƶ������֮ǰ�����л�Ƶ��֡�ﵽһ��ֵ
        *pitch_avail_inbuf_count >= PITCH_AVERAGE_UPDATE_THRD) {	//��ʼ����ǰ�����������ƽ����Ƶ
        valid_increment = PitchAverage(pitch_mean, *pitch_valid_len,	//����ֵΪ��һ����������Ч��Ƶ������
                                       pitch_record, *pitch_avail_inbuf_count);
        *pitch_valid_len += valid_increment;					//��¼��ʷ����Ч��Ƶ������
		if ((*pitch_valid_len > 100) || (*pitch_valid_len < 0))
		{
			*pitch_valid_len = 100;
		}
        for (i = 0; i < *pitch_avail_inbuf_count; i++) {		//����¼��Ƶ���������
            pitch_record[i] = 0.0f;
        }
        *pitch_avail_inbuf_count = 0;							//��ռ�¼��Ƶ������
        if (*pitch_mean_ready == 0) {							//��ʾ��Ƶ��ƽ��ֵ������ϣ�ֻ�ڵ�һ�ν���ʱ��ִ�����
            *pitch_mean_ready = 1;
        }
    }
}
