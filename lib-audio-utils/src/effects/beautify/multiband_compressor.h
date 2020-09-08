#ifndef MULTIBAND_COMPRESSOR_H
#define MULTIBAND_COMPRESSOR_H

enum MulCompressMode {
    // 原声
    MulComNone = 0,
    // 清晰人声
    MulComCleanVoice,
    // 低音->沉稳
    MulComBass,
    // 低沉
    MulComLowVoice,
    // 穿透
    MulComPenetrating,
    // 磁性
    MulComMagnetic,
    // 柔和高音
    MulComSoftPitch
};

typedef struct MulCompressorT MulCompressor;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建多频段压缩器
 *
 * @param sample_rate
 * @return MulCompressor*
 */
MulCompressor* MulCompressorCreate(const int sample_rate);

/**
 * @brief 释放多频段压缩器
 *
 * @param inst
 */
void MulCompressorFree(MulCompressor** inst);

/**
 * @brief 设置音效模式
 *
 * @param inst
 * @param mode
 */
void MulCompressorSetMode(MulCompressor* inst, const enum MulCompressMode mode);

/**
 * @brief 处理数据
 *
 * @param inst
 * @param buffer 输入输出数据
 * @param buffer_size 数据长度
 * @return int 处理后的数据长度
 */
int MulCompressorProcess(MulCompressor* inst, float* buffer,
                         const int buffer_size);

#ifdef __cplusplus
}
#endif

#endif