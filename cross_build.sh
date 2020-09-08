#!/usr/bin/env bash

#--------------------
set -e
set +x

UNAME_S=$(uname -s)
UNAME_SM=$(uname -sm)
echo "build on $UNAME_SM"

###################### android  ##########################
build_android() {
    ANDROID_COMMON_ARGS="-DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake -DANDROID_TOOLCHAIN=gcc -DCMAKE_BUILD_TYPE=$1"
    ##### android armv5
    mkdir -p out/$1/build-android-armeabi
    pushd out/$1/build-android-armeabi
    cmake $ANDROID_COMMON_ARGS -DANDROID_ABI="armeabi" -DANDROID_PLATFORM=android-9 ../../../lib-audio-utils
    make
    make install
    popd

    ##### android armv7
    mkdir -p out/$1/build-android-armeabi-v7a
    pushd out/$1/build-android-armeabi-v7a
    cmake $ANDROID_COMMON_ARGS -DANDROID_ABI="armeabi-v7a" -DANDROID_ARM_NEON=ON -DANDROID_PLATFORM=android-16 ../../../lib-audio-utils
    make
    make install
    popd

    ##### android aarch64
    mkdir -p out/$1/build-android-arm64-v8a
    pushd out/$1/build-android-arm64-v8a
    cmake $ANDROID_COMMON_ARGS -DANDROID_ABI="arm64-v8a" -DANDROID_PLATFORM=android-21 ../../../lib-audio-utils
    make
    make install
    popd

    ##### android x86
    mkdir -p out/$1/build-android-x86
    pushd out/$1/build-android-x86
    cmake $ANDROID_COMMON_ARGS -DANDROID_ABI="x86" -DANDROID_PLATFORM=android-21 ../../../lib-audio-utils
    make
    make install
    popd

    ##### android x86_64
    mkdir -p out/$1/build-android-x86_64
    pushd out/$1/build-android-x86_64
    cmake $ANDROID_COMMON_ARGS -DANDROID_ABI="x86_64" -DANDROID_PLATFORM=android-21 ../../../lib-audio-utils
    make
    make install
    popd
}

######################  ios ##############################
build_ios() {
    IOS_COMMON_ARGS="-DCMAKE_TOOLCHAIN_FILE=../../../toolchains/ios.toolchain.cmake -DCMAKE_BUILD_TYPE=$1"
    ##### ios simulator i386
    mkdir -p out/$1/build-ios-sim
    pushd out/$1/build-ios-sim
    cmake $IOS_COMMON_ARGS -DIOS_PLATFORM=SIMULATOR ../../../lib-audio-utils
    make
    make install
    popd

    ##### ios simulator x86_64
    mkdir -p out/$1/build-ios-sim64
    pushd out/$1/build-ios-sim64
    cmake $IOS_COMMON_ARGS -DIOS_PLATFORM=SIMULATOR64 ../../../lib-audio-utils
    make
    make install
    popd

    ##### "OS" to build for Device (armv7, armv7s, arm64, arm64e)
    ##### ios armv7 armv7s arm64 arm64e
    mkdir -p out/$1/build-ios
    pushd out/$1/build-ios
    cmake $IOS_COMMON_ARGS -DIOS_PLATFORM=OS ../../../lib-audio-utils
    make
    make install
    popd
}

build_wrap() {
    case "$UNAME_S" in
        Linux)
            echo "ANDROID_NDK=$ANDROID_NDK"
            if [ ! -z "$ANDROID_NDK" ]; then
                echo "build android"
                build_android $1
            else
                echo_usage
                exit 1
            fi
        ;;
        Darwin)
            echo "ANDROID_NDK=$ANDROID_NDK"
            if [ ! -z "$ANDROID_NDK" ]; then
                echo "build android"
                build_android $1
            fi
            if xcode-select --install 2>&1 | grep installed; then
                echo "build ios"
                build_ios $1
            else
                echo_usage
                exit 1
            fi
        ;;
        *)
            echo "unsupport host os.pls use mac or linux"
            exit 1
        ;;
    esac
}

echo_usage() {
    echo "Usage:"
    echo "  cross_build.sh Debug/Release"
    echo "      on linux build android only"
    echo "      on mac can build android and ios"
    echo "  cross_build.sh clean"
    echo "      clean compile file"
    echo "  cross_build.sh help"
    echo "      display this"
    exit 1
}

TARGET=$1
case $TARGET in
    clean)
        echo "remove out folder"
        rm -rf out
    ;;
    help)
        echo_usage
    ;;
    Debug|Release)
        echo "build start"
        build_wrap $1
        echo "build end"
    ;;
    *)
        echo "build start"
        build_wrap Release
        echo "build end"
    ;;
esac
