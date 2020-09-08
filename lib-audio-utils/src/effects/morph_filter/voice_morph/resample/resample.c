#include "libavutil/channel_layout.h"
#include "libavutil/opt.h"
#include "libavutil/samplefmt.h"
#include "libswresample/swresample.h"

#include <stddef.h>
#include "resample.h"

int VoiceMorph_AudioResample_Create(struct SwrContext** rsmp_inst) {
    *rsmp_inst = swr_alloc();
    if (!rsmp_inst) {
        return -1;
    }

    return 0;
}

int VoiceMorph_AudioResample_Init(struct SwrContext* rsmp_inst,
                                  short in_frame_len, int src_rate,
                                  unsigned char*** rsmp_input,
                                  unsigned char*** rsmp_output,
                                  int* out_frame_len, int* out_linesize,
                                  int* initialized) {
    int ret, src_linesize;

    if (*initialized) {
        if (rsmp_input != NULL) {
            av_freep(*rsmp_input);		//�ͷ�rsmp_inputָ���ָ��
            av_freep(rsmp_input);		//�ͷ�rsmp_input����
        }

        if (rsmp_output != NULL) {
            av_freep(*rsmp_output);
            av_freep(rsmp_output);
        }
    }

    ret = av_samples_alloc_array_and_samples(
        rsmp_input, &src_linesize, 1, in_frame_len, AV_SAMPLE_FMT_FLT, 0);		//������Ƶ��ʽ������Ӧ��С���ڴ�ռ�
    if (ret < 0) {
        return -1;
    }

    *out_frame_len = av_rescale_rnd(in_frame_len, 44100, src_rate, AV_ROUND_UP);	//����a*b/c��ȡ�����൱�ڼ����ԭ������ΪFs��һ�β�������Ϊa������ת��Ϊ44100�����Ժ󣬲���������Ϊ����
    ret = av_samples_alloc_array_and_samples(
        rsmp_output, out_linesize, 1, *out_frame_len, AV_SAMPLE_FMT_S16, 0);
    if (ret < 0) {
        return -1;
    }

    /* set options */
    av_opt_set_int(rsmp_inst, "in_channel_layout", AV_CH_LAYOUT_MONO, 0);		//����ͨ��
    av_opt_set_int(rsmp_inst, "in_sample_rate", src_rate, 0);					//��������
    av_opt_set_sample_fmt(rsmp_inst, "in_sample_fmt", AV_SAMPLE_FMT_FLT, 0);	//������ʽ

    av_opt_set_int(rsmp_inst, "out_channel_layout", AV_CH_LAYOUT_MONO, 0);
    av_opt_set_int(rsmp_inst, "out_sample_rate", 44100, 0);
    av_opt_set_sample_fmt(rsmp_inst, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);	//����ǰ���Ƶ�ز������Լ���Ҫ�Ĳ�����

    if ((ret = swr_init(rsmp_inst)) < 0) {
        return -1;
    }

    *initialized = 1;

    return 0;
}

int VoiceMorph_AudioResample_Process(struct SwrContext* rsmp_inst,
                                     unsigned char** rsmp_input,
                                     int in_frame_len,
                                     unsigned char** rsmp_output,
                                     int* out_frame_len, int src_rate,
                                     int* out_linesize) {
    int dst_bufsize;

    *out_frame_len = swr_get_out_samples(rsmp_inst, in_frame_len);

    *out_frame_len =
        swr_convert(rsmp_inst, rsmp_output, *out_frame_len,
                    (const unsigned char**)rsmp_input, in_frame_len);
    if (*out_frame_len < 0) {
        return -1;
    }

    dst_bufsize = av_samples_get_buffer_size(out_linesize, 1, *out_frame_len,
                                             AV_SAMPLE_FMT_S16, 1);
    if (dst_bufsize < 0) {
        return -1;
    }

    return dst_bufsize;
}

void VoiceMorph_AudioResample_Release(struct SwrContext* rsmp_inst,
                                      unsigned char** rsmp_input,
                                      unsigned char** rsmp_output) {
    if (rsmp_input != NULL) {
        av_freep(&rsmp_input[0]);
        av_freep(&rsmp_input);
    }

    if (rsmp_output != NULL) {
        av_freep(&rsmp_output[0]);
        av_freep(&rsmp_output);
    }

    if (rsmp_inst != NULL) {
        swr_free(&rsmp_inst);
    }
}
