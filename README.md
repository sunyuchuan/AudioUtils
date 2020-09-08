# AudioUtils
音频编辑工具——音频特效混音等

根据json配置lib-audio-utils/data/effect_config.txt，加特效并多轨道混音，通过ffmpeg或mediacodec生成m4a文件


Linux  :
    cd lib-audio-utils
    ./run_test.sh

Android:
    Android Need [NDK r14b](https://dl.google.com/android/repository/android-ndk-r14b-linux-x86_64.zip)
    ./cross_build.sh
    ./package.sh
    cd android/audio_utils-armv7a/src/main/jni/
    ndk-build
    cd -
    cd android/audio_utils-armv5/src/main/jni/
    ndk-build
    cd android/audio_utils-arm64/src/main/jni/
    ndk-build
    cd android/audio_utils-x86/src/main/jni/
    ndk-build
    cd android/audio_utils-x86_64/src/main/jni/
    ndk-build
    install the apk to android,now you can use the score on android

