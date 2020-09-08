#!/bin/bash
rm -rf out
./cross_build.sh clean
./cross_build.sh
./package.sh
# 压力测试
# adb shell monkey -p com.layne.audioeffect -s 100 --throttle 500 -vvv 10000 > monkey10000.txt
# 定位crash
# adb logcat | ndk-stack -sym android/AudioEffect/libeffect/src/main/obj/local/armeabi-v7a/