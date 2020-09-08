/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef _NOISE_SUPPRESSION_H_
#define _NOISE_SUPPRESSION_H_
#include <stdint.h>

typedef struct NsHandleT NsHandle;

#ifdef __cplusplus
extern "C" {
#endif

// log level
typedef enum NsMode {
    NS_MODE_LEVEL_0 = 0,
    NS_MODE_LEVEL_1,
    NS_MODE_LEVEL_2,
    NS_MODE_LEVEL_3
} NsMode;

/*
 * This function creates an instance to the noise suppression structure
 *
 * Input:
 *      - NS_inst       : Pointer to noise suppression instance that should be
 *                        created
 *
 * Output:
 *      - NS_inst       : Pointer to created noise suppression instance
 *
 * Return value         :  0 - Ok
 *                        -1 - Error
 */
int XmNs_Create(NsHandle** NS_inst);


/*
 * This function frees the dynamic memory of a specified noise suppression
 * instance.
 *
 * Input:
 *      - NS_inst       : Pointer to NS instance that should be freed
 *
 * Return value         :  0 - Ok
 *                        -1 - Error
 */
int XmNs_Free(NsHandle* NS_inst);


/*
 * This function initializes a NS instance and has to be called before any other
 * processing is made.
 *
 * Input:
 *      - NS_inst       : Instance that should be initialized
 *      - fs            : sampling frequency
 *
 * Output:
 *      - NS_inst       : Initialized instance
 *
 * Return value         :  0 - Ok
 *                        -1 - Error
 */
int XmNs_Init(NsHandle* NS_inst, uint32_t fs);


/*
 * This changes the aggressiveness of the noise suppression method.
 *
 * Input:
 *      - NS_inst       : Noise suppression instance.
 *      - mode          : 0: Mild, 1: Medium , 2: Aggressive
 *
 * Output:
 *      - NS_inst       : Updated instance.
 *
 * Return value         :  0 - Ok
 *                        -1 - Error
 */
int XmNs_set_policy(NsHandle* NS_inst, enum NsMode mode);


/*
 * This functions does Noise Suppression process
 *
 * Input
 *      - NS_inst       : Noise suppression instance.
 *      - shBufferIn       : Pointer to input frame buffer
 *      - in_len     : input frame buffer length
 *
 * Output:
 *      - shBufferOut      : Pointer to output buffer
 *      - out_len    : output frame length
 *
 * Return value         : The length of the data obtained
 *                        Less than 0 - Error or in_len < 80
 */
int XmNS_Process(NsHandle *pNS_inst, short *shBufferIn, int in_len,
    short *shBufferOut, int out_len);


/*
 * This functions does Refresh the output buffer
 *
 * Input
 *      - pNS_inst       : Noise suppression instance.
 *
 * Output:
 *      - shBufferOut      : Pointer to output buffer
 *      - out_len    : output frame length
 *
 * Return value         : The length of the data obtained
 *                        Less than 0 - end
 */
int XmNs_Flush(NsHandle *pNS_inst, short *shBufferOut, int out_len);


#ifdef __cplusplus
}
#endif

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_NS_INCLUDE_NOISE_SUPPRESSION_H_
