#ifndef LIMITER_H_
#define LIMITER_H_

typedef struct LimiterT Limiter;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建限制器
 *
 * @return Limiter*
 */
Limiter* LimiterCreate(const int sample_rate);

/**
 * @brief 释放限制器
 *
 * @param inst
 */
void LimiterFree(Limiter** inst);

void LimiterSetSwitch(Limiter* inst, const int limiter_switch);

void LimiterSet(Limiter* inst, const float limiter_threshold_in_dB,
                const float attack_time_in_ms, const float decay_time_in_ms,
                const float output_gain_in_dB);

/**
 * @brief 处理声音
 *
 * @param inst
 * @param buffer 输入输出数据
 * @param buffer_size 数据长度
 */
int LimiterProcess(Limiter* inst, float* buffer, const int buffer_size);

#ifdef __cplusplus
}
#endif
#endif  //  LIMITER_H_
