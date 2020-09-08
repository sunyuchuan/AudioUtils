#ifndef _MORPH_RESAMPLE_H_
#define _MORPH_RESAMPLE_H_

int VoiceMorph_AudioResample_Create(struct SwrContext** rsmp_inst);
int VoiceMorph_AudioResample_Init(struct SwrContext* rsmp_inst,
                                  short in_frame_len, int src_rate,
                                  unsigned char*** rsmp_input,
                                  unsigned char*** rsmp_output,
                                  int* out_frame_len, int* out_linesize,
                                  int* initialized);
int VoiceMorph_AudioResample_Process(struct SwrContext* rsmp_inst,
                                     unsigned char** rsmp_input,
                                     int in_frame_len,
                                     unsigned char** rsmp_output,
                                     int* out_frame_len, int src_rate,
                                     int* out_linesize);
void VoiceMorph_AudioResample_Release(struct SwrContext* rsmp_inst,
                                      unsigned char** rsmp_input,
                                      unsigned char** rsmp_output);

#endif
