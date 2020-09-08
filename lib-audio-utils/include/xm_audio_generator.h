#if defined(__ANDROID__) || defined (__linux__)

#ifndef XM_AUDIO_GENERATOR_H_
#define XM_AUDIO_GENERATOR_H_

typedef struct XmAudioGenerator XmAudioGenerator;

enum GeneratorStatus {
    GS_ERROR = -1,
    GS_COMPLETED,
    GS_STOPPED,
};

#define GENERATOR_STATE_UNINIT  0
#define GENERATOR_STATE_INITIALIZED  1
#define GENERATOR_STATE_STARTED  2
#define GENERATOR_STATE_STOP  3
#define GENERATOR_STATE_COMPLETED  4
#define GENERATOR_STATE_ERROR  5

/**
 * @brief Reference count subtract 1
 *
 * @param self XmAudioGenerator
 */
void xmag_dec_ref(XmAudioGenerator *self);

/**
 * @brief Reference count subtract 1
 *
 * @param self XmAudioGenerator
 */
void xmag_dec_ref_p(XmAudioGenerator **self);

/**
 * @brief Reference count plus 1
 *
 * @param self XmAudioGenerator
 */
void xmag_inc_ref(XmAudioGenerator *self);

/**
 * @brief free XmAudioGenerator
 *
 * @param self XmAudioGenerator*
 */
void xm_audio_generator_free(XmAudioGenerator *self);

/**
 * @brief free XmAudioGenerator
 *
 * @param self XmAudioGenerator**
 */
void xm_audio_generator_freep(XmAudioGenerator **self);

/**
 * @brief stop generator
 *
 * @param self XmAudioGenerator
 */
void xm_audio_generator_stop(XmAudioGenerator *self);

/**
 * @brief get progress of generator
 *
 * @param self XmAudioGenerator
 */
int xm_audio_generator_get_progress(XmAudioGenerator *self);

/**
 * @brief startup add voice effects and mix voice\bgm\music
 *
 * @param self XmAudioGenerator
 * @param in_config_path Config file about audio mix parameter
 * @param out_file_path Output audio file path
 * @param encode_type 0:ffmpeg encoder,1:mediacodec encoder
 * @return GeneratorStatus
 */
enum GeneratorStatus xm_audio_generator_start(
        XmAudioGenerator *self, const char *in_config_path,
        const char *out_file_path, int encode_type);

/**
 * @brief create XmAudioGenerator
 *
 * @return XmAudioGenerator*
 */
XmAudioGenerator *xm_audio_generator_create();

#endif
#endif
