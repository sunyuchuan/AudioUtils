#ifndef EQUALIZER_H
#define EQUALIZER_H

#ifdef __cplusplus
extern "C" {
#endif

enum EqualizerMode {
    // 原声
    EqNone = 0,
    // 清晰人声
    EqCleanVoice,
    // 低音 -> 沉稳
    EqBass,
    // 低沉 -> 低音
    EqLowVoice,
    // 穿透
    EqPenetrating,
    // 磁性
    EqMagnetic,
    // 柔和高音
    EqSoftPitch
};

typedef struct EqualizerT Equalizer;

/**
 * @brief 创建均衡器
 *
 * @return Equalizer*
 */
Equalizer* EqualizerCreate(const int sample_rate);

/**
 * @brief 释放均衡器
 *
 * @param inst
 */
void EqualizerFree(Equalizer** inst);

/**
 * @brief 设置音效模式
 *
 * @param inst
 * @param mode
 */
void EqualizerSetMode(Equalizer* inst, const enum EqualizerMode mode);

/**
 * @brief 处理声音
 *
 * @param inst
 * @param buffer
 * @param buffer_size
 */
void EqualizerProcess(Equalizer* inst, float* buffer, const int buffer_size);

#ifdef __cplusplus
}
#endif

#endif