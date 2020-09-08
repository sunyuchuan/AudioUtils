//
// Created by layne on 19-4-27.
//

#ifndef AUDIO_EFFECT_ERROR_DEF_H_
#define AUDIO_EFFECT_ERROR_DEF_H_

#define AUDIO_EFFECT_SUCCESS 0
#define AUDIO_EFFECT_EOF -1
#define AEERROR_NULL_POINT -1008
#define AEERROR_INVALID_PARAMETER -2000
#define AEERROR_INVALID_STATE -2001
#define AEERROR_NOMEM -5000
#define AEERROR_INVAL -5001
#define AEERROR_EXTERNAL -6000

#if defined(__ANDROID__) || defined (__linux__)
#include "libavutil/error.h"

#define kSampleRateNotSupport -1000
#define kChannelsNotSupport -1001
#define kSwitchError -1002
#define kAacOpenError -1003
#define kAddBgmInfoError -1004
#define kOpenLogFileError -1005
#define kBufferSizeError -1006
#define kBgmFileNameIsNull -1007
#define kNullPointError -1008
#define kNoMemory AVERROR(ENOMEM)
#define kNoMuxer AVERROR_MUXER_NOT_FOUND
#define kNoProtocal AVERROR_PROTOCOL_NOT_FOUND
#define kNoDemuxer AVERROR_DEMUXER_NOT_FOUND
#define kNoAudioStrem AVERROR_STREAM_NOT_FOUND
#define kDecoderNotFound AVERROR_DECODER_NOT_FOUND
#define kEndOfFile -7000
#define kUnknowError AVERROR_UNKNOWN
#define kLogErrorInvalidData AVERROR_INVALIDDATA
#endif

#endif  // AUDIO_EFFECT_ERROR_DEF_H_
