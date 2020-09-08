/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "noise_suppression.h"
#include <stdlib.h>
#include <string.h>
#include "ns/typedefs.h"
#include "ns/ns_core.h"
#include "ns/signal_processing_library.h"
#include "ns/defines.h"
#include "tools/fifo.h"

#define BUFFER_LEN_320 320
#define BUFFER_LEN_160 160

int XmNs_Create(NsHandle** NS_inst) {
	*NS_inst = (NsHandle*) calloc(1, sizeof(NSinst_t));
	if (*NS_inst != NULL) {
        (*(NSinst_t**)NS_inst)->initFlag = 0;
        int size = BUFFER_LEN_320 * sizeof(short);
        (*(NSinst_t**)NS_inst)->shInL = (short *)calloc(1, size);
        (*(NSinst_t**)NS_inst)->shInH = (short *)calloc(1, size);
        (*(NSinst_t**)NS_inst)->shOutL = (short *)calloc(1, size);
        (*(NSinst_t**)NS_inst)->shOutH = (short *)calloc(1, size);
        return 0;
    }

	return -1;
}

int XmNs_Init(NsHandle* NS_inst, uint32_t fs) {
	return WebRtcNs_InitCore((NSinst_t *) NS_inst, fs);
}

int XmNs_set_policy(NsHandle* NS_inst, enum NsMode mode) {
	return WebRtcNs_set_policy_core((NSinst_t *) NS_inst, mode);
}

int XmNs_Flush(NsHandle *pNS_inst,short *shBufferOut, int out_len) {
	int ret = 0;
	NSinst_t *p = (NSinst_t *)pNS_inst;
	short *data = NULL;
	int len = fifo_occupancy(p->f_in);
	if (len > 0)
	{
		data = (short*)malloc(len * sizeof(short));
		memset(data, 0, len * sizeof(short));
		fifo_read(p->f_in, data, len);
		fifo_write(p->f_out, data, len);
		free(data);
	}
	ret = fifo_read(p->f_out, shBufferOut, out_len);
	return ret;
}

int XmNs_Free(NsHandle* NS_inst) {
	NSinst_t *p = (NSinst_t *)NS_inst;
	if (p->f_in) fifo_delete(&(p->f_in));
	if (p->f_out) fifo_delete(&(p->f_out));
	if (p->shInL) free(p->shInL);
	if (p->shInH) free(p->shInH);
	if (p->shOutL) free(p->shOutL);
	if (p->shOutH) free(p->shOutH);
	free(p);
	return 0;
}

/*
 * This functions does Noise Suppression for the inserted speech frame. The
 * input and output signals should always be 10ms (80 or 160 samples).
 *
 * Input
 *      - NS_inst       : Noise suppression instance.
 *      - spframe       : Pointer to speech frame buffer for L band
 *      - spframe_H     : Pointer to speech frame buffer for H band
 *      - fs            : sampling frequency
 *
 * Output:
 *      - NS_inst       : Updated NS instance
 *      - outframe      : Pointer to output frame for L band
 *      - outframe_H    : Pointer to output frame for H band
 *
 * Return value         :  0 - OK
 *                        -1 - Error
 */
static int Ns_Process_in(NsHandle* NS_inst, short* spframe,
	short* spframe_H, short* outframe, short* outframe_H) {
	return WebRtcNs_ProcessCore(
		(NSinst_t*) NS_inst, spframe, spframe_H, outframe, outframe_H);
}

static void NS_Process_H(NsHandle *pNS_inst,
	short *shBufferIn, short *shBufferOut) {
	NSinst_t *p = (NSinst_t *)pNS_inst;
	memset(p->shInL, 0, BUFFER_LEN_320 * sizeof(short));
	memset(p->shInH, 0, BUFFER_LEN_320 * sizeof(short));
	memset(p->shOutL, 0, BUFFER_LEN_320 * sizeof(short));
	memset(p->shOutH, 0, BUFFER_LEN_320 * sizeof(short));

	XmWebRtcSpl_AnalysisQMF(shBufferIn, p->shInL, p->shInH,
		p->filter_state1, p->filter_state12);
	if (!Ns_Process_in(pNS_inst, p->shInL, p->shInH, p->shOutL, p->shOutH)) {
		short tmpout[BUFFER_LEN_320] = { 0 };
		XmWebRtcSpl_SynthesisQMF(p->shOutL, p->shOutH,
			tmpout, p->Synthesis_state1, p->Synthesis_state12);
		memcpy(shBufferOut, tmpout, BUFFER_LEN_320 * sizeof(short));
	}
}

static void NS_Process_L(NsHandle *pNS_inst,
	short *shBufferIn, short *shBufferOut, int in_size) {
	NSinst_t *p = (NSinst_t *)pNS_inst;
	memset(p->shInL, 0, BUFFER_LEN_320 * sizeof(short));
	memset(p->shInH, 0, BUFFER_LEN_320 * sizeof(short));
	memset(p->shOutL, 0, BUFFER_LEN_320 * sizeof(short));
	memset(p->shOutH, 0, BUFFER_LEN_320 * sizeof(short));

	int size = in_size * sizeof(short);
	memcpy(p->shInL, (short *)(shBufferIn), size);
	if (!Ns_Process_in(pNS_inst, p->shInL, p->shInH, p->shOutL, p->shOutH)) {
		memcpy(shBufferOut, p->shOutL, size);
	}
}

int XmNS_Process(NsHandle *pNS_inst, short *shBufferIn,
	int in_len, short *shBufferOut, int out_len) {
	int ret = 0;
	short buffer_in[BUFFER_LEN_320] = { 0 };
	short buffer_out[BUFFER_LEN_320] = { 0 };
	NSinst_t *p = (NSinst_t *)pNS_inst;
	int fs = p->fs;

	fifo_write(p->f_in, shBufferIn, in_len);

	if (fs == 8000 || fs == 16000) {
		size_t buffer_size = fs * 0.01;
		while (fifo_occupancy(p->f_in) > buffer_size) {
			fifo_read(p->f_in, buffer_in, buffer_size);
			NS_Process_L((NsHandle *)pNS_inst, buffer_in, buffer_out, buffer_size);
			fifo_write(p->f_out, buffer_out, buffer_size);
		}
	} else {
		size_t buffer_size = BUFFER_LEN_320;
		while (fifo_occupancy(p->f_in) > buffer_size) {
			fifo_read(p->f_in, buffer_in, buffer_size);
			NS_Process_H((NsHandle *)pNS_inst, buffer_in, buffer_out);
			fifo_write(p->f_out, buffer_out, buffer_size);
		}
	}

	ret = fifo_read(p->f_out, shBufferOut, out_len);
	return ret;
}
