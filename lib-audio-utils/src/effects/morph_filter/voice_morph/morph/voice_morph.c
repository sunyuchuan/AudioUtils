#ifdef __cplusplus
extern "C" {
#endif

#include "voice_morph.h"
#include <stdlib.h>
#include <string.h>
#include "pitch_tracker/src/pitch_macro.h"
#include "log.h"
#include "morph_core.h"
#include "voice_morph/resample/resample.h"

VoiceMorph* VoiceMorph_Create(char *file_name) {
    LogInfo("%s\n", __func__);
    VoiceMorph *self = (VoiceMorph *)calloc(1, sizeof(VoiceMorph));
    if (NULL == self) {
        LogError("%s alloc VoiceMorph failed.\n", __func__);
        goto fail;
    }

    self->pitch = PitchTracker_Create();
    if (self->pitch == NULL) {
        goto fail;
    }

    if (VoiceMorph_AudioResample_Create(&self->swr_ctx) == -1) {
        goto fail;
    }

    if (VoiceMorph_AudioResample_Init(
            self->swr_ctx, RESAMPLE_FRAME_INPUT_LEN, LOCAL_SAMPLE_RATE_FIXED,
            &self->rsmp_in_buf, &self->rsmp_out_buf, &self->rsmp_out_len,
            &self->rsmp_out_linesize, &self->resample_initialized) == -1) {
        goto fail;
    }

    self->morph_inbuf_float = (float *)malloc(sizeof(float) * PITCH_FRAME_SHIFT * 10);
    if (self->morph_inbuf_float == NULL) {
        goto fail;
    }

    self->morph_buf = (float *)malloc(sizeof(float) * PITCH_FRAME_SHIFT * 9);
    if (self->morph_buf == NULL) {
        goto fail;
    }

    self->seg_pitch_primary = (float *)malloc(sizeof(float) * 7);
    if (self->seg_pitch_primary == NULL) {
        goto fail;
    }

    self->seg_pitch_new = (float *)malloc(sizeof(float) * 7);
    if (self->seg_pitch_new == NULL) {
        goto fail;
    }

    self->pitch_peak = (short *)malloc(sizeof(short) * PITCH_FRAME_SHIFT);  // Fs/Fmin*(1.25-0.8)
    if (self->pitch_peak == NULL) {
        goto fail;
    }

    self->last_joint_buf =
        (float *)malloc(sizeof(float) * LAST_JOINT_LEN);  // Fs/Fmin
    if (self->last_joint_buf == NULL) {
        goto fail;
    }

    self->last_fall_buf = (float *)malloc(sizeof(float) * PITCH_BUFFER_LENGTH);
    if (self->last_fall_buf == NULL) {
        goto fail;
    }

    self->out_buf = (float *)malloc(sizeof(float) * PITCH_BUFFER_LENGTH * 3);
    if (self->out_buf == NULL) {
        goto fail;
    }
	//rebuild_factor = (refactor*)malloc(sizeof(refactor));

    return self;
fail:
    VoiceMorph_Release(&self);
    return NULL;
}

int VoiceMorph_Init(VoiceMorph *self) {
    LogInfo("%s\n", __func__);
    if (!self) return -1;

    if (PitchTracker_Init(self->pitch, 450.0f, 75.0f,
            0.5f, 0.1f, MAX_CAND_NUM, 0.02f) == -1) {
        return -1;
    }

    memset(self->morph_inbuf_float, 0, sizeof(float) * PITCH_FRAME_SHIFT * 10);
    memset(self->morph_buf, 0, sizeof(float) * PITCH_FRAME_SHIFT * 9);
    memset(self->seg_pitch_primary, 0, sizeof(float) * 7);
    memset(self->seg_pitch_new, 0, sizeof(float) * 7);
    memset(self->pitch_peak, 0, sizeof(short) * PITCH_FRAME_SHIFT);
    memset(self->last_joint_buf, 0, sizeof(float) * PITCH_BUFFER_LENGTH / 2);
    memset(self->last_fall_buf, 0, sizeof(float) * PITCH_BUFFER_LENGTH);
    memset(self->out_buf, 0, sizeof(float) * PITCH_BUFFER_LENGTH * 2);
	memset(&self->rebuild_factor, 0x0, sizeof(refactor));
	self->rebuild_factor.fake_time_num = -1;
    self->morph_buf_pos = 8 * PITCH_FRAME_SHIFT;
    self->prev_peak_pos = 4 * PITCH_FRAME_SHIFT;
    self->src_acc_pos = 4 * PITCH_FRAME_SHIFT;
    self->seg_pitch_transform = 0.0f;
    self->formant_ratio = 1.0f;
    self->pitch_ratio = 1.0f;
    self->pitch_range_factor = 1.0f;
    self->last_fall_len = PITCH_BUFFER_LENGTH;
    self->out_len = 0;

    return 0;
}

int VoiceMorph_SetConfig(VoiceMorph *self, float pitch_coeff) {
    LogInfo("%s pitch_coeff %f.\n", __func__, pitch_coeff);
    if (!self) return -1;

    if (pitch_coeff > 2.0f || pitch_coeff < 0.5f) {
        return -1;
    }

    self->pitch_ratio = pitch_coeff;
    self->formant_ratio = VoiceMorphGetPitchFactor(pitch_coeff);

    int src_rate = (int)roundf(self->formant_ratio * LOCAL_SAMPLE_RATE_FLOAT);
    VoiceMorph_AudioResample_Init(self->swr_ctx, RESAMPLE_FRAME_INPUT_LEN, src_rate,
        &self->rsmp_in_buf, &self->rsmp_out_buf, &self->rsmp_out_len,
        &self->rsmp_out_linesize, &self->resample_initialized);

    return 0;
}

int VoiceMorph_Process(VoiceMorph *self, void *raw, short in_size,
    char *morph_out, int * morph_out_size, bool robot) {
    if (!self || !raw || !morph_out || !morph_out_size) return -1;

	refactor *re_factor = &self->rebuild_factor;
	short i, k, j, total_len,
        shift_count = 0, in_pos = 0, res_len, swr_total_len, swr_res_len,
        swr_outsize, loop_count, buf_pos = 0, in_len, *in = (short *)raw;
	int output_size_rsmp = 0;
	char morph_out_rsmp[10000] = { 0 };
	if ((self->morph_factor != 0 && self->morph_factor != self->formant_ratio) || (self->morph_factor != 0 && self->robot_status != robot))
	{
		int link_out_size = 0;
		memset(self->seg_pitch_primary, 0, sizeof(float) * 7);
		memset(self->seg_pitch_new, 0, sizeof(float) * 7);
		self->pitch_range_factor = self->formant_ratio;
		/*reset morph related parameters*/
		self->prev_peak_pos = 4 * PITCH_FRAME_SHIFT;
		self->src_acc_pos = 4 * PITCH_FRAME_SHIFT;
		self->last_fall_len = PITCH_BUFFER_LENGTH;

		link_calculate(re_factor,&link_out_size, self->formant_ratio, &self->rsmp_in_len_count);
		*morph_out_size += link_out_size*2;
		memset(morph_out, 0,*morph_out_size);
	}
	else if (re_factor->total_out_len>= RESET_NUM)
	{
		int link_out_size = 0;
		memset(self->seg_pitch_primary, 0, sizeof(float) * 7);
		memset(self->seg_pitch_new, 0, sizeof(float) * 7);
		self->pitch_range_factor = self->formant_ratio;
		/*reset morph related parameters*/
		self->prev_peak_pos = 4 * PITCH_FRAME_SHIFT;
		self->src_acc_pos = 4 * PITCH_FRAME_SHIFT;
		self->last_fall_len = PITCH_BUFFER_LENGTH;

		link_calculate(re_factor, &link_out_size, self->formant_ratio, &self->rsmp_in_len_count);
		*morph_out_size += link_out_size * 2;
		memset(morph_out, 0, *morph_out_size);
	}
	self->morph_factor = self->formant_ratio;
	self->robot_status = robot;
    int src_rate = (int)(LOCAL_SAMPLE_RATE_FLOAT * self->formant_ratio);
    if (in_size < 0) {
        return -1;
    }
    in_len = in_size >> 1;

    if (PitchTracker_Process(self->pitch, in, in_len, self->morph_inbuf_float,
            self->seg_pitch_primary, self->seg_pitch_new) == -1) {
        return -1;
    }

    total_len = self->morph_buf_pos - 8 * PITCH_FRAME_SHIFT + in_len;
    if (in_len < PITCH_FRAME_SHIFT) {
        if (total_len <
            PITCH_FRAME_SHIFT)  // fewer than a shift-length, just copy
        {
            memcpy(&self->morph_buf[self->morph_buf_pos], self->morph_inbuf_float, in_len);
            self->morph_buf_pos += in_len;
            return 0;
        } else {
            shift_count = 1;
        }
    } else {
        shift_count = total_len / PITCH_FRAME_SHIFT;
    }

    for (i = 0; i < shift_count; i++) {
        self->out_len = 0;
        memcpy(&self->morph_buf[self->morph_buf_pos], &self->morph_inbuf_float[in_pos],
               (MORPH_BUF_LEN - self->morph_buf_pos) * sizeof(float));
        in_pos += (MORPH_BUF_LEN - self->morph_buf_pos);
        self->morph_buf_pos = MORPH_BUF_LEN - PITCH_FRAME_SHIFT;

        if (robot) {
            if (self->pitch->pitch_cand_buf[i] != 0.0f) {
                self->seg_pitch_transform = 175.0f;
            } else {
                self->seg_pitch_transform = 0.0f;
            }
        } else {
            VoiceMorphPitchTransform(self->pitch->pitch_cand_buf[i], self->pitch_ratio,
                                    self->pitch_range_factor, &self->seg_pitch_transform);
        }
        self->seg_pitch_primary[0] = self->seg_pitch_primary[1];
        self->seg_pitch_primary[1] = self->seg_pitch_primary[2];
        self->seg_pitch_primary[2] = self->seg_pitch_primary[3];
        self->seg_pitch_primary[3] = self->seg_pitch_primary[4];
		self->seg_pitch_primary[4] = self->pitch->pitch_cand_buf[i];
		self->seg_pitch_new[0] = self->seg_pitch_new[1];
		self->seg_pitch_new[1] = self->seg_pitch_new[2];
		self->seg_pitch_new[2] = self->seg_pitch_new[3];
		self->seg_pitch_new[3] = self->seg_pitch_new[4];
		self->seg_pitch_new[4] = self->seg_pitch_transform;
        self->prev_peak_pos -= PITCH_FRAME_SHIFT;
        self->src_acc_pos -= PITCH_FRAME_SHIFT;

        VoiceMorphPitchStretch(re_factor, self->morph_buf, self->seg_pitch_new,
            self->seg_pitch_primary, &self->prev_peak_pos, self->formant_ratio,
            self->pitch->sample_rate, &self->src_acc_pos, self->out_buf,
            &self->out_len, self->pitch_peak);

        memmove(self->morph_buf, &self->morph_buf[PITCH_FRAME_SHIFT],
                (MORPH_BUF_LEN - PITCH_FRAME_SHIFT) * sizeof(float));

        swr_total_len = self->rsmp_in_len_count + self->out_len;
        if (swr_total_len >= RESAMPLE_FRAME_INPUT_LEN) {
            loop_count = swr_total_len / RESAMPLE_FRAME_INPUT_LEN;
            buf_pos = 0;
            for (k = 0; k < loop_count; k++) {
                memcpy(&self->rsmp_in_buf[0][self->rsmp_in_len_count << 2],
                    &self->out_buf[buf_pos],
                    sizeof(float) *
                        (RESAMPLE_FRAME_INPUT_LEN - self->rsmp_in_len_count));
                swr_outsize = VoiceMorph_AudioResample_Process(
                    self->swr_ctx, self->rsmp_in_buf, RESAMPLE_FRAME_INPUT_LEN,
                    self->rsmp_out_buf, &self->rsmp_out_len,
                    src_rate, &self->rsmp_out_linesize);
                if (swr_outsize < 0) {
                    return -1;
                }
                memcpy(&morph_out_rsmp[output_size_rsmp], self->rsmp_out_buf[0],
                    swr_outsize);
				output_size_rsmp += swr_outsize;
                buf_pos += (RESAMPLE_FRAME_INPUT_LEN - self->rsmp_in_len_count);
                self->rsmp_in_len_count = 0;
            }
            self->rsmp_in_len_count =
                swr_total_len - loop_count * RESAMPLE_FRAME_INPUT_LEN;
            memcpy(self->rsmp_in_buf[0], &self->out_buf[buf_pos],
                   sizeof(float) * self->rsmp_in_len_count);
        } else {
            memcpy(&self->rsmp_in_buf[0][self->rsmp_in_len_count << 2],
                self->out_buf, sizeof(float) * self->out_len);
            self->rsmp_in_len_count = swr_total_len;
        }
    }
	re_factor->real_out_count += output_size_rsmp /2;
    // residual data copy
    res_len = total_len - shift_count * PITCH_FRAME_SHIFT;
    memcpy(&self->morph_buf[self->morph_buf_pos],
        &self->morph_inbuf_float[in_pos], res_len * sizeof(float));
    self->morph_buf_pos += res_len;

    self->pitch->pitch_cand_buf_len = 0;
	memcpy(&morph_out[*morph_out_size], &morph_out_rsmp[0],
		output_size_rsmp);
	*morph_out_size += output_size_rsmp;

    return 0;
}

void VoiceMorph_Release(VoiceMorph **vMorph) {
    if (!vMorph || !*vMorph) return;

    VoiceMorph *self = *vMorph;
    if (self->pitch != NULL) {
        PitchTracker_Release(&self->pitch);
    }

    VoiceMorph_AudioResample_Release(self->swr_ctx,
        self->rsmp_in_buf, self->rsmp_out_buf);

    if (self->morph_inbuf_float != NULL) {
        free(self->morph_inbuf_float);
    }

    if (self->morph_buf != NULL) {
        free(self->morph_buf);
    }

    if (self->seg_pitch_primary != NULL) {
        free(self->seg_pitch_primary);
    }

    if (self->seg_pitch_new != NULL) {
        free(self->seg_pitch_new);
    }

    if (self->pitch_peak != NULL) {
        free(self->pitch_peak);
    }

    if (self->last_joint_buf != NULL) {
        free(self->last_joint_buf);
    }

    if (self->last_fall_buf != NULL) {
        free(self->last_fall_buf);
    }

    if (self->out_buf != NULL) {
        free(self->out_buf);
    }

    free(*vMorph);
    *vMorph = NULL;
}

#ifdef __cplusplus
}
#endif
