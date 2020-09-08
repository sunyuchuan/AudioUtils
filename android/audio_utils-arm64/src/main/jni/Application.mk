APP_OPTIM := release
APP_PLATFORM := android-21
APP_ABI := arm64-v8a
NDK_TOOLCHAIN_VERSION=4.9
APP_PIE := false

APP_CFLAGS := -O3 -Wall -pipe \
    -ffast-math \
    -fstrict-aliasing -Werror=strict-aliasing \
    -Wno-psabi -Wa,--noexecstack \
    -DANDROID -DNDEBUG
