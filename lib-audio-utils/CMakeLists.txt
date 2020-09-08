# CMake Options
cmake_minimum_required(VERSION 3.5.1)
if(POLICY CMP0025)
  cmake_policy(SET CMP0025 NEW)
endif()
if(POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif()
if(POLICY CMP0054)
  cmake_policy(SET CMP0054 NEW)
endif()

project(audio_utils C)

add_definitions(-D_GNU_SOURCE -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D_POSIX_C_SOURCE=200809L)

set(CMAKE_C_EXTENSIONS      ON)
set(CMAKE_C_STANDARD        99)
if(${CMAKE_C_COMPILER_ID} STREQUAL "MSVC")
elseif(${CMAKE_C_COMPILER_ID} MATCHES "^(GNU|.*Clang)$")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror=implicit -std=gnu99 -fPIC")

    if(${CMAKE_C_COMPILER_ID} STREQUAL "GNU" AND ${CMAKE_C_COMPILER_VERSION} VERSION_GREATER "6.0.0")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wnarrowing -Werror=discarded-qualifiers -Werror=incompatible-pointer-types")
    endif()

    if(${CMAKE_C_COMPILER_ID} STREQUAL "Clang" OR ${CMAKE_C_COMPILER_ID} STREQUAL "AppleClang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wnarrowing -Werror=ignored-qualifiers -Werror=incompatible-pointer-types")
    endif()

    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fomit-frame-pointer")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fomit-frame-pointer -s")

    message(STATUS "gcc/clang cflags ${CMAKE_C_FLAGS}")
    message(STATUS "gcc/clang cflags debug ${CMAKE_C_FLAGS_DEBUG}")
    message(STATUS "gcc/clang cflags release ${CMAKE_C_FLAGS_RELEASE}")
endif()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ftree-vectorize -ffast-math -O3 -Wno-unused-variable -Wno-unused-parameter")

message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/src/effects)
include_directories(${PROJECT_SOURCE_DIR}/src/effects/morph_filter)

# ffmpeg
if(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
if(${ANDROID_ABI} STREQUAL "armeabi")
    set(FFmpeg_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/../prebuilt/android/ffmpeg-armv5/include)
elseif(${ANDROID_ABI} STREQUAL "armeabi-v7a")
    set(FFmpeg_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/../prebuilt/android/ffmpeg-armv7a/include)
elseif(${ANDROID_ABI} STREQUAL "arm64-v8a")
    set(FFmpeg_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/../prebuilt/android/ffmpeg-arm64/include)
elseif(${ANDROID_ABI} STREQUAL "x86")
    set(FFmpeg_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/../prebuilt/android/ffmpeg-x86/include)
elseif(${ANDROID_ABI} STREQUAL "x86_64")
    set(FFmpeg_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/../prebuilt/android/ffmpeg-x86_64/include)
else()
    set(FFmpeg_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/../prebuilt/android/ffmpeg-armv7a/include)
endif()
    include_directories(${FFmpeg_INCLUDE_DIRS})
    include_directories(${PROJECT_SOURCE_DIR}/src/codec/mediacodec)
elseif(IOS)
    set(FFmpeg_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/../prebuilt/ios/)
    include_directories(${FFmpeg_INCLUDE_DIRS})
else()
    # include system lib
    include_directories("/usr/local/include")
    link_directories("/usr/local/lib")
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
SET(MEDIACODEC
src/codec/mediacodec/xm_android_jni.c
src/codec/mediacodec/ijkj4a/Build.c
src/codec/mediacodec/ijkj4a/ByteBuffer.util.c
src/codec/mediacodec/ijkj4a/ByteBuffer.c
src/codec/mediacodec/ijkj4a/j4a_allclasses.c
src/codec/mediacodec/ijkj4a/j4a_base.c
src/codec/mediacodec/ijkj4a/MediaCodec.c
src/codec/mediacodec/ijkj4a/MediaFormat.c

src/codec/mediacodec/ijksdl/ijksdl_codec_android_mediacodec.c
src/codec/mediacodec/ijksdl/ijksdl_codec_android_mediacodec_internal.c
src/codec/mediacodec/ijksdl/ijksdl_codec_android_mediacodec_java.c
src/codec/mediacodec/ijksdl/ijksdl_codec_android_mediadef.c
src/codec/mediacodec/ijksdl/ijksdl_codec_android_mediaformat.c
src/codec/mediacodec/ijksdl/ijksdl_codec_android_mediaformat_java.c
src/codec/mediacodec/ijksdl/ijksdl_mutex.c
src/codec/mediacodec/ijksdl/ijksdl_timer.c
src/codec/mediacodec/ijksdl/ijksdl_thread.c
src/codec/mediacodec/audio_encoder_mediacodec.c
)
elseif(IOS)
SET(MEDIACODEC )
else()
SET(MEDIACODEC
src/codec/mediacodec/ijksdl/ijksdl_thread.c
)
endif()

add_library(${PROJECT_NAME}
${MEDIACODEC}
src/codec/ffmpeg_utils.c
src/codec/ffmpeg_decoder.c
src/codec/audio_encoder.c
src/codec/sw/audio_encoder_sw.c
src/codec/audio_muxer.c
src/codec/pcm_decoder.c
src/codec/idecoder.c
src/codec/audio_decoder_factory.c
src/codec/duration_parser.c

src/tools/avstring.c
src/tools/conversion.c
src/tools/dict.c
src/tools/fifo.c
src/tools/log.c
#src/tools/mem.c
src/tools/sdl_mutex.c
src/tools/util.c

src/effects/echo.c
src/effects/echos.c
src/effects/reverb.c

src/effects/dsp_tools/fft/fft8g.c
src/effects/dsp_tools/iir_design/iir_design.c
src/effects/beautify/compressor.c
src/effects/beautify/equalizer.c
src/effects/beautify/flanger.c
src/effects/beautify/limiter.c
src/effects/beautify/multiband_compressor.c
src/effects/beautify.c

src/effects/morph_filter/pitch_tracker/src/low_pass.c
src/effects/morph_filter/pitch_tracker/src/pitch_core.c
src/effects/morph_filter/pitch_tracker/src/pitch_tracker.c
src/effects/morph_filter/voice_morph/morph/morph_core.c
src/effects/morph_filter/voice_morph/morph/voice_morph.c
src/effects/morph_filter/voice_morph/resample/resample.c
src/effects/morph_filter.c

src/effects/resample/resample.c
src/effects/minions.c

src/effects/math/junior_func.c
src/effects/noise_suppression/ns/fft4g.c
src/effects/noise_suppression/ns/splitting_filter.c
src/effects/noise_suppression/ns/ns_core.c
src/effects/noise_suppression/noise_suppression.c
src/effects/noise_suppression.c
src/effects/volume_limiter.c
src/effects/voice_effect.c
src/effects/xm_audio_effects.c

src/mixer/fade_in_out.c
src/mixer/side_chain_compress.c
src/mixer/xm_audio_mixer.c

src/json/cJSON.c
src/json/json_parse.c

src/source/audio_source.c
src/source/audio_source_queue.c

src/wave/wav_dec.c

src/xm_wav_utils.c
src/xm_duration_parser.c
src/xm_audio_utils.c
src/xm_audio_generator.c
)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
elseif(IOS)
else()
    target_link_libraries(${PROJECT_NAME} m pthread avformat avcodec avutil swresample bsd)
endif()

# set prefix as global var
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install")

install(DIRECTORY include DESTINATION .)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    )

target_include_directories(${PROJECT_NAME}
                           PRIVATE src
                           PUBLIC include)

if(ENABLE_TESTING AND NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Android" AND NOT IOS)
  add_subdirectory(tests)
endif()
