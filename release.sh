#! /usr/bin/env bash

date_suffix=`date +%Y%m%d%H%M%S`
hash=`git rev-parse --short HEAD`

echo "clean"
rm -rf out
./cross_build.sh clean
echo "build static library"
./cross_build.sh
echo "copy static library to android"
./package.sh

echo "compile jni"
cd android/audio_utils-armv7a/src/main/jni/
ndk-build
cd -
cd android/audio_utils-armv5/src/main/jni/
ndk-build
cd -
cd android/audio_utils-arm64/src/main/jni/
ndk-build
cd -
cd android/audio_utils-x86/src/main/jni/
ndk-build
cd -
cd android/audio_utils-x86_64/src/main/jni/
ndk-build
cd -

echo "release shared library"
mkdir libs
cp -a android/audio_utils-armv5/src/main/libs/*  libs
cp -a android/audio_utils-armv7a/src/main/libs/*  libs
cp -a android/audio_utils-arm64/src/main/libs/* libs
cp -a android/audio_utils-x86/src/main/libs/*  libs
cp -a android/audio_utils-x86_64/src/main/libs/*    libs

echo "compress libs"
tar zcvf libs-${date_suffix}-${hash}.tar.gz libs

rm -rf libs
