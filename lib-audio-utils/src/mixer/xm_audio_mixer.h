#ifndef XM_AUDIO_MIXER_H_
#define XM_AUDIO_MIXER_H_

typedef struct XmMixerContext_T XmMixerContext;

#define MIX_STATE_UNINIT  0
#define MIX_STATE_INITIALIZED  1
#define MIX_STATE_STARTED  2
#define MIX_STATE_COMPLETED  3
#define MIX_STATE_ERROR  4

/**
 * @brief free XmMixerContext
 *
 * @param ctx
 */
void xm_audio_mixer_freep(XmMixerContext **ctx);

/**
 * @brief stop mix
 *
 * @param ctx
 */
void xm_audio_mixer_stop(XmMixerContext *ctx);

/**
 * @brief get progress
 *
 * @param ctx
 */
int xm_audio_mixer_get_progress(XmMixerContext *ctx);

/**
 * @brief get mixed frame
 *
 * @param ctx XmMixerContext
 * @param buffer buffer for storing data
 * @param buffer_size_in_short buffer size
 * @return size of valid buffer obtained.
        Less than or equal to 0 means failure or end
 */
int xm_audio_mixer_get_frame(XmMixerContext *ctx,
    short *buffer, int buffer_size_in_short);

/**
 * @brief mixer seekTo
 *
 * @param ctx XmMixerContext
 * @param seek_time_ms seek target time in ms
 * @return Less than 0 means failure
 */
int xm_audio_mixer_seekTo(XmMixerContext *ctx,
    int seek_time_ms);

/**
 * @brief mix bgm\music and output m4a
 *
 * @param ctx XmMixerContext
 * @param out_file_path output file path
 * @param encoder_type Support ffmpeg and HW
 * @return Less than 0 means failure
 */
int xm_audio_mixer_mix(XmMixerContext *ctx,
    const char *out_file_path, int encoder_type);

/**
 * @brief mixer init
 *
 * @param ctx XmMixerContext
 * @param in_config_path Config file about bgm and music parameter
 * @return Less than 0 means failure
 */
int xm_audio_mixer_init(XmMixerContext *ctx,
    const char *in_config_path);

/**
 * @brief create XmMixerContext
 *
 * @param sample_rate The sample rate of output audio
 * @param channels The channels of output audio
 * @return XmMixerContext*
 */
XmMixerContext *xm_audio_mixer_create();

#endif
