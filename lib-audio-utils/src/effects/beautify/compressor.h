#ifndef COMPRESSOR_H
#define COMPRESSOR_H

typedef struct CompressorT Compressor;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建压缩器
 *
 * @param sample_rate 输入数据的采样率
 * @return Compressor*
 */
Compressor* CompressorCreate(const int sample_rate);

/**
 * @brief 释放压缩器
 *
 * @param inst
 */
void CompressorFree(Compressor** inst);

/**
 * @brief 设置参数
 *
 * @param inst
 * @param compressor_threshold_in_dB 压缩门限值
 * @param ratio 压缩比
 * @param attack_time_in_ms 触发时间（毫秒）
 * @param decay_time_in_ms 释放时间（毫秒）
 * @param output_gain_in_dB 输出增益（dB）
 */
void CompressorSet(Compressor* inst, const float compressor_threshold_in_dB,
                   const float ratio, const float attack_time_in_ms,
                   const float decay_time_in_ms, const float output_gain_in_dB);

/**
 * @brief 数据处理
 *
 * @param inst
 * @param buffer 输入输出数据
 * @param buffer_size 数据大小
 * @return int
 */
int CompressorProcess(Compressor* inst, float* buffer, const int buffer_size);

#ifdef __cplusplus
}
#endif

#endif