#ifdef __cplusplus
extern "C" {
#endif

#include "pitch_tracker.h"
#include "low_pass.h"
#include "pitch_core.h"
#include "pitch_macro.h"
#include "log.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

PitchTracker* PitchTracker_Create() {
    PitchTracker *self = (PitchTracker *)calloc(1, sizeof(PitchTracker));
    if (NULL == self) {
        LogError("%s alloc PitchTracker failed.\n", __func__);
        goto fail;
    }

    self->sample_rate = 44100.0f;

    self->pitch_buf = (float *)malloc(sizeof(float) * PITCH_BUFFER_LENGTH);
    if (self->pitch_buf == NULL) {
        goto fail;
    }

    self->frame_shift_buf = (float *)malloc(sizeof(float) * PITCH_FRAME_SHIFT);
    if (self->frame_shift_buf == NULL) {
        goto fail;
    }

    self->pitch_cand_buf =
        (float *)malloc(sizeof(float) * 10);  // 15680 Bytes input maximum
    if (self->pitch_cand_buf == NULL) {
        goto fail;
    }

    self->long_term_pitch_record =
        (float *)malloc(sizeof(float) * LONG_TERM_PITCH_REFERENCE_LEN);
    if (self->long_term_pitch_record == NULL) {
        goto fail;
    }

    self->autocorr_buf = (float *)malloc(sizeof(float) * PITCH_BUFFER_LENGTH);
    if (self->autocorr_buf == NULL) {
        goto fail;
    }

    self->freq_cand_seq = (float *)malloc(sizeof(float) * MAX_CAND_NUM);
    if (self->freq_cand_seq == NULL) {
        goto fail;
    }

    self->intensity_cand_seq = (float *)malloc(sizeof(float) * MAX_CAND_NUM);
    if (self->intensity_cand_seq == NULL) {
        goto fail;
    }

    self->win_pitch_frame = (float *)malloc(sizeof(float) * PITCH_BUFFER_LENGTH);
    if (self->win_pitch_frame == NULL) {
        goto fail;
    }

    return self;
fail:
    PitchTracker_Release(&self);
    return NULL;
}

int PitchTracker_Init(PitchTracker* self,
    float freq_upper_bound, float freq_lower_bound,
    float voice_max_power, float intens_min_power,
    short candidate_max_num, float frame_peak_thrd) {
    if (!self) return -1;
    short i, j;

    if (freq_upper_bound >= FREQUENCY_FLOOR &&
        freq_upper_bound <= FREQUENCY_CEIL) {
        self->max_freq = freq_upper_bound;
    } else {
        return -1;
    }
    if (freq_lower_bound >= FREQUENCY_FLOOR &&
        freq_lower_bound <= FREQUENCY_CEIL) {
        self->min_freq = freq_lower_bound;
    } else {
        return -1;
    }

    if (voice_max_power > 0.0f) {
        self->voice_thrd = voice_max_power;
    } else {
        return -1;
    }

    if (intens_min_power > 0.0f) {
        self->intens_thrd = intens_min_power;
    } else {
        return -1;
    }

    if (candidate_max_num <= MAX_CAND_NUM && candidate_max_num > 0) {
        self->cand_num = candidate_max_num;
    } else {
        return -1;
    }

    if (frame_peak_thrd > 0.0f) {
        self->local_peak_thrd = frame_peak_thrd;
    } else {
        return -1;
    }

    memset(self->pitch_buf, 0, sizeof(float) * PITCH_BUFFER_LENGTH);
    memset(self->frame_shift_buf, 0, sizeof(float) * PITCH_FRAME_SHIFT);
    memset(self->long_term_pitch_record, 0,
           sizeof(float) * LONG_TERM_PITCH_REFERENCE_LEN);
    memset(self->autocorr_buf, 0, sizeof(float) * PITCH_BUFFER_LENGTH);
    memset(self->freq_cand_seq, 0, sizeof(float) * MAX_CAND_NUM);
    memset(self->intensity_cand_seq, 0, sizeof(float) * MAX_CAND_NUM);
    memset(self->pitch_cand_buf, 0, sizeof(float) * 10);
    memset(self->win_pitch_frame, 0, sizeof(float) * PITCH_BUFFER_LENGTH);

    self->max_lag = ceil(self->sample_rate / self->min_freq);
    self->min_lag = floor(self->sample_rate / self->max_lag);

    self->long_term_pitch = 0.0f;
    self->long_term_pitch_count = 0;
    self->long_term_pitch_ready = 0;
    self->pitch_counting_flag = 0;
    self->successive_pitch_seg_count = 0;
    self->pitch_inbuf_count = 0;
    self->pitch_cand_buf_len = 0;

	memset(self->peak_record, 0, sizeof(float) * 10);
	self->peak_sum=0;
	self->peak_count=0;
	self->peak_avg = 0;
    return 0;
}

int PitchTracker_Process(PitchTracker* self,
    short *in, short in_len, float *float_buf,
    float *seg_pitch_primary, float *seg_pitch_new) {
    if (!self || !in || !float_buf
        || !seg_pitch_primary || !seg_pitch_new) return -1;

	short test_low[PITCH_BUFFER_LENGTH - PITCH_FRAME_SHIFT] = { 0 };
    short shift_count = 0, i, n, k, in_pos = 0, lowpass_len, win_frame_len,
        total_len, buf_init_pos, float_buf_pos = 0;
    float local_peak, best_cand, d;
	short local_peak_pos;
    int to_send_size;
	int zero_pass_count = 0;

	short low_pass_buf[PITCH_FRAME_SHIFT] = { 0 };
    if (in_len < 0 || in == NULL || in_len > 3920) {
        return -1;
    }
    total_len = in_len + self->shift_buf_pos;
    buf_init_pos = self->shift_buf_pos;
    if (in_len < PITCH_FRAME_SHIFT) {
        if (total_len < PITCH_FRAME_SHIFT) {
            // fewer than a shift-length, just copy
            for (n = 0; n < in_len; n++) {
                d = ((float)in[n]) * 0.000030517578f;
                self->frame_shift_buf[self->shift_buf_pos + n] = d;
                float_buf[n] = d;
            }
            self->shift_buf_pos += in_len;
            return 0;
        } else {
            shift_count = 1;
        }
    } else {
        shift_count = total_len / PITCH_FRAME_SHIFT;
    }

    // process data
    for (i = 0; i < shift_count; i++) {
        self->cand_num = 0;
        for (n = 0; n < (PITCH_FRAME_SHIFT - self->shift_buf_pos); n++) {
            d = ((float)in[n + in_pos]) * 0.000030517578f;
            self->frame_shift_buf[self->shift_buf_pos + n] = d;
            float_buf[float_buf_pos + n] = d;
        }
        float_buf_pos += (PITCH_FRAME_SHIFT - self->shift_buf_pos);
        in_pos = (PITCH_FRAME_SHIFT - buf_init_pos) + i * PITCH_FRAME_SHIFT;
        self->shift_buf_pos = 0;

        memmove(self->pitch_buf, &self->pitch_buf[PITCH_FRAME_SHIFT],
                (PITCH_BUFFER_LENGTH - PITCH_FRAME_SHIFT) * sizeof(float));
        LowPassIIR(self->frame_shift_buf, PITCH_FRAME_SHIFT,
                    &self->pitch_buf[PITCH_BUFFER_LENGTH - PITCH_FRAME_SHIFT],
                    &lowpass_len);
		/*for (int k = 0; k < PITCH_FRAME_SHIFT;k++)
		{
			low_pass_buf[k] = (short)(32767.0*pitch_buf[k]);
		}

		fwrite(low_pass_buf, sizeof(short), PITCH_FRAME_SHIFT, test_fp);*/

		zero_pass_count=zero_pass(self->frame_shift_buf, PITCH_FRAME_SHIFT);
		local_peak = FindLocalPitchPeak(self->pitch_buf, PITCH_FRAME_SHIFT);
		self->mute_count++;
        if ((local_peak > self->local_peak_thrd) && zero_pass_count<200) {
			self->local_peak_thrd = (self->local_peak_thrd*2*0.95+ local_peak * 0.05)*0.5;
			self->mute_count = 0;
            DedirectAndWindow(self->pitch_buf, PITCH_BUFFER_LENGTH, self->win_pitch_frame,
                                &win_frame_len);
            AutoCorrelation(self->win_pitch_frame, self->autocorr_buf);
            FindPitchCand(self->autocorr_buf, self->freq_cand_seq, self->intensity_cand_seq,
                            self->max_lag, self->min_lag, self->voice_thrd, self->intens_thrd,
                            self->sample_rate, MAX_CAND_NUM, &self->cand_num, self->win_pitch_frame);
        }
		if (self->mute_count == 100)
		{
			self->local_peak_thrd = self->local_peak_thrd * 0.5;
			self->mute_count = 0;
		}
        best_cand = SelectBestPitchCand(
            self->intensity_cand_seq, self->freq_cand_seq, self->cand_num, &self->prev_pitch,
            &self->successive_confidence, &self->last_pitch_flag, self->min_freq, self->max_freq,
            self->long_term_pitch_ready, self->long_term_pitch, seg_pitch_primary,seg_pitch_new, self->pitch_cand_buf, &self->un_confidence);
		//peak_avg = Peak_avg_update(best_cand, &peak_count, peak_record, local_peak, &peak_sum);
        self->pitch_cand_buf[self->pitch_cand_buf_len] = best_cand;
        self->pitch_cand_buf_len++;
        LongTermPitchEsitmate(best_cand, &self->pitch_counting_flag,
                                &self->successive_pitch_seg_count, &self->pitch_inbuf_count,
                                &self->long_term_pitch_ready, self->long_term_pitch_record,
                                &self->long_term_pitch, &self->long_term_pitch_count);
    }
    self->shift_buf_pos = total_len - shift_count * PITCH_FRAME_SHIFT;
    for (k = 0; k < self->shift_buf_pos; k++) {
        d = ((float)in[in_pos + k]) * 0.000030517578f;
        self->frame_shift_buf[k] = d;
        float_buf[float_buf_pos + k] = d;
    }
    return 0;
}

void PitchTracker_Release(PitchTracker** pTracker) {
    if (!pTracker || !*pTracker) return;

    PitchTracker *self = *pTracker;
    if (self->pitch_buf != NULL) {
        free(self->pitch_buf);
        self->pitch_buf = NULL;
    }

    if (self->frame_shift_buf != NULL) {
        free(self->frame_shift_buf);
        self->frame_shift_buf = NULL;
    }

    if (self->long_term_pitch_record != NULL) {
        free(self->long_term_pitch_record);
        self->long_term_pitch_record = NULL;
    }

    if (self->autocorr_buf != NULL) {
        free(self->autocorr_buf);
        self->autocorr_buf = NULL;
    }

    if (self->freq_cand_seq != NULL) {
        free(self->freq_cand_seq);
        self->freq_cand_seq = NULL;
    }

    if (self->intensity_cand_seq != NULL) {
        free(self->intensity_cand_seq);
        self->intensity_cand_seq = NULL;
    }

    if (self->win_pitch_frame != NULL) {
        free(self->win_pitch_frame);
        self->win_pitch_frame = NULL;
    }

    if (self->pitch_cand_buf != NULL) {
        free(self->pitch_cand_buf);
        self->pitch_cand_buf = NULL;
    }

    free(*pTracker);
    *pTracker = NULL;
}

#ifdef __cplusplus
}
#endif
