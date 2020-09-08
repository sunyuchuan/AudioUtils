#ifndef FLANGER_H_
#define FLANGER_H_

typedef enum { INTERP_LINEAR, INTERP_QUADRATIC } interp_t;
typedef enum { WAVE_SINE, WAVE_TRIANGLE } wave_t;

typedef struct FlangerT Flanger;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建镶边效果器
 *
 * @param sample_rate 输入数据的采样率
 * @return Flanger*
 */
Flanger* FlangerCreate(const int sample_rate);

/**
 * @brief 释放镶边效果器
 *
 * @param inst
 */
void FlangerFree(Flanger** inst);

/**
 * @brief 设置参数
 *
 * @param inst 效果器实例
 * @param delay 基本延迟(0～30毫秒)
 * @param depth 增加扫描延迟（0～10毫秒）
 * @param regen 再生百分比（延迟信号反馈 -95～+95）
 * @param width 与原始信号混合的延迟信号百分比（0~100）
 * @param speed 每秒扫描次数（0.1～10Hz）
 * @param shape 扫频波形：正弦/三角形
 * @param phase 扫频波相移百分比(0～100)
 */
void FlangerSet(Flanger* inst, const float delay, const float depth,
                const float regen, const float width, const float speed,
                const wave_t shape, const float phase);

/**
 * @brief 处理数据
 *
 * @param inst
 * @param buffer 数据
 * @param buffer_size 数据大小
 */
void FlangerProcess(Flanger* inst, float* buffer, const int buffer_size);

#ifdef __cplusplus
}
#endif
#endif  // FLANGER_H_
