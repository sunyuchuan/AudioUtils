#if defined(__ANDROID__) || defined (__linux__)

#ifndef FFMPEG_UTILS_H
#define FFMPEG_UTILS_H

#include <stdbool.h>
#include "libavformat/avformat.h"
#include "libavutil/audio_fifo.h"
#include "libavutil/opt.h"
// #include "libavutil/timestamp.h"
#include "libswresample/swresample.h"

#ifndef MAX_NB_SAMPLES
#define MAX_NB_SAMPLES 1024
#endif
#define FRAME_SIZE 1024

extern void SetFFmpegLogLevel(int log_level);

extern void RegisterFFmpeg();

extern int CopyString(const char* src, char** dst);

extern void InitPacket(AVPacket* packet);

extern int CheckSampleRateAndChannels(const int sample_rate_in_Hz,
                                      const int nb_channels);

extern int OpenInputMediaFile(AVFormatContext** fmt_ctx, const char* filename);

#if defined(__ANDROID__) || defined (__linux__)
extern int FindFirstStream(AVFormatContext* fmt_ctx, enum AVMediaType type);

extern int FindBestStream(AVFormatContext* fmt_ctx, enum AVMediaType type);
#else
extern int FindFirstStream(AVFormatContext* fmt_ctx, int type);

extern int FindBestStream(AVFormatContext* fmt_ctx, int type);
#endif

extern int AllocDecodeFrame(AVFrame** decode_frame);

extern double GetStreamStartTime(AVFormatContext* fmt_ctx,
                                 const unsigned int stream_index);

extern int FindAndOpenDecoder(const AVFormatContext* fmt_ctx,
                              AVCodecContext** dec_ctx, const int stream_index);

extern void InitAdtsHeader(uint8_t* adts_header, const int sample_rate_in_Hz,
                           const int nb_channels);

extern void ResetAdtsHeader(uint8_t* adts_header, const int nb_channels,
                            const int packet_size);

extern int AddAudioStream(AVFormatContext *ofmt_ctx, AVCodecContext *enc_ctx);

extern int FindAndOpenAudioEncoder(AVCodecContext** enc_ctx,
                                   const enum AVCodecID codec_id,
                                   const int bit_rate, const int nb_channels,
                                   const int sample_rate_in_Hz);

extern int FindAndOpenH264Encoder(AVCodecContext** enc_ctx, const int bit_rate,
                                  const int frame_rate, const int gop_size,
                                  const int width, const int height,
                                  const enum AVPixelFormat pix_fmt);

extern int AddStreamToFormat(AVFormatContext* fmt_ctx, AVCodecContext* enc_ctx);

extern int WriteFileHeader(AVFormatContext* ofmt_ctx, const char* filename);

extern void WriteFileTrailer(AVFormatContext* ofmt_ctx);

extern int InitResampler(const int src_channels, const int dst_channels,
                         const int src_sample_rate, const int dst_sample_rate,
                         const enum AVSampleFormat src_sample_fmt,
                         const enum AVSampleFormat dst_sample_fmt,
                         SwrContext** swr_ctx);

extern int AllocEncodeAudioFrame(AVFrame** audio_frame, const int nb_channels,
                                 const int sample_rate_in_Hz,
                                 const int nb_samples,
                                 const enum AVSampleFormat sample_fmt);

extern int AllocEncodeVideoFrame(AVFrame** video_frame, const int width,
                                 const int height,
                                 const enum AVPixelFormat pix_fmt);

extern int AllocateSampleBuffer(uint8_t*** buffer, const int nb_channels,
                                const int nb_samples,
                                const enum AVSampleFormat sample_fmt);

extern int AllocAudioFifo(const enum AVSampleFormat sample_fmt,
                          const int nb_channels, AVAudioFifo** encode_fifo);

extern int AudioFifoPut(AVAudioFifo* fifo, const int nb_samples, void** buffer);

extern int AudioFifoGet(AVAudioFifo* fifo, const int nb_samples, void** buffer);

extern void AudioFifoReset(AVAudioFifo* fifo);

// extern void LogPacket(const AVFormatContext* fmt_ctx, const AVPacket* pkt);

extern float UpdateFactorS16(const float factor, const int sum);

extern short GetSumS16(const int sum);

extern void MixBufferS16(const short* src_buffer1, const short* src_buffer2,
                         const int nb_mix_samples, const int nb_channels,
                         short* dst_buffer, float* left_factor,
                         float* right_factor);

extern void StereoToMonoS16(short* dst, short* src, const int nb_samples);

extern void MonoToStereoS16(short* dst, short* src, const int nb_samples);

#endif  // FFMPEG_UTILS_H
#endif // defined(__ANDROID__) || defined (__linux__)
