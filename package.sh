#!/usr/bin/env bash

#--------------------
set -e
set +x

UNAME_S=$(uname -s)
UNAME_SM=$(uname -sm)
echo "package on $UNAME_SM"

NAME=audio-utils

##### package android lib
package_android() {
    if [ -d out/Release ]; then
        ANDROIDPKGNAME=${NAME}-android-lib
        rm -rf $ANDROIDPKGNAME

        echo "create folder"
        mkdir -p $ANDROIDPKGNAME
        mkdir -p $ANDROIDPKGNAME/armeabi
        mkdir -p $ANDROIDPKGNAME/armeabi-v7a
        mkdir -p $ANDROIDPKGNAME/arm64-v8a
        mkdir -p $ANDROIDPKGNAME/x86
        mkdir -p $ANDROIDPKGNAME/x86_64
        mkdir -p $ANDROIDPKGNAME/include

        echo "copy effect lib"
        cp -a out/Release/build-android-armeabi/install/lib       $ANDROIDPKGNAME/armeabi/
        cp -a out/Release/build-android-armeabi-v7a/install/lib   $ANDROIDPKGNAME/armeabi-v7a/
        cp -a out/Release/build-android-arm64-v8a/install/lib     $ANDROIDPKGNAME/arm64-v8a/
        cp -a out/Release/build-android-x86/install/lib           $ANDROIDPKGNAME/x86/
        cp -a out/Release/build-android-x86_64/install/lib        $ANDROIDPKGNAME/x86_64/

        echo "copy ffmpeg lib"
        cp prebuilt/android/ffmpeg-armv5/libijkffmpeg-armeabi.so             $ANDROIDPKGNAME/armeabi/
        cp prebuilt/android/ffmpeg-armv7a/libijkffmpeg-armeabi-v7a.so        $ANDROIDPKGNAME/armeabi-v7a/
        cp prebuilt/android/ffmpeg-arm64/libijkffmpeg-arm64-v8a.so           $ANDROIDPKGNAME/arm64-v8a/
        cp prebuilt/android/ffmpeg-x86/libijkffmpeg-x86.so                   $ANDROIDPKGNAME/x86/
        cp prebuilt/android/ffmpeg-x86_64/libijkffmpeg-x86_64.so             $ANDROIDPKGNAME/x86_64/

        echo "copy header"
        cp -r out/Release/build-android-arm64-v8a/install/include/* $ANDROIDPKGNAME/include/

        # echo "move $ANDROIDPKGNAME to sdk"
        rm -rf android/lib-audio-utils/$ANDROIDPKGNAME
        mv $ANDROIDPKGNAME android/lib-audio-utils/
    else
        echo "Need Run ./cross_build.sh Release Frist !"
    fi
}

##### package ios framework
package_ios() {
    IOSPKGNAME=${NAME}.framework
    rm -rf $IOSPKGNAME
    mkdir -p $IOSPKGNAME/Versions/A/Headers
    mkdir -p $IOSPKGNAME/Versions/A/Resources
    ln -s A $IOSPKGNAME/Versions/Current
    ln -s Versions/Current/Headers $IOSPKGNAME/Headers
    ln -s Versions/Current/Resources $IOSPKGNAME/Resources
    ln -s Versions/Current/${NAME} $IOSPKGNAME/${NAME}
    lipo -create \
        out/Release/build-ios/install/lib/lib${NAME}.a       \
        out/Release/build-ios-sim64/install/lib/lib${NAME}.a \
        out/Release/build-ios-sim/install/lib/lib${NAME}.a   \
        -o $IOSPKGNAME/Versions/A/${NAME}
    cp -r out/Release/build-ios/install/include/* $IOSPKGNAME/Versions/A/Headers/
    cp ios/Info.plist ${IOSPKGNAME}/Versions/A/Resources/
    rm -f $IOSPKGNAME.zip
    zip -9 -y -r $IOSPKGNAME.zip $IOSPKGNAME
    echo "rm framework"
    rm -rf ios/dubscore/Frameworks/$IOSPKGNAME
    echo "copy new framework"
    mv $IOSPKGNAME ios/dubscore/Frameworks
}

case "$UNAME_S" in
    Linux)
        echo "package android on linux"
        package_android
    ;;
    Darwin)
        echo "package android on mac"
        package_android
        echo "package ios on mac"
        package_ios
    ;;
esac
