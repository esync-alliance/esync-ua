#!/bin/bash

CMK=$(type -p cmake)
if test -d ${HOME}/cmake-3.8/bin/cmake; then
    CMK=${HOME}/cmake-3.8/bin/cmake
fi
if test -n "$CMAKE_38"; then
    CMK="$CMAKE_38"
fi

TCR=${HOME}/tmp/linaro

if test -n "$USE_ARM_TOOLCHAIN"; then
    TCR="$USE_ARM_TOOLCHAIN"
fi

TCH=arm-linux-gnueabi
TCP="${TCR}/bin/${TCH}-"
export CC=${TCP}gcc
export SYSROOT="${TCR}/sysroot"

export CROSS_COMPILE=${TCH}
export PROJECT_ROOT=`pwd`
export CFLAGS="-I$SYSROOT/usr/include -I$PROJECT_ROOT/arm/include"
export LDFLAGS="-L$SYSROOT/usr/lib -lm -lz -L$PROJECT_ROOT/arm/lib "

export PKG_CONFIG_PATH="${SYSROOT}/usr/lib/pkgconfig"

rm -rf arm_build
mkdir -p arm_build

(cd arm_build && {
    LIBZIP_DIR="${SYSROOT}" ${CMK} -DCMAKE_FIND_ROOT_PATH=${SYSROOT} -DCMAKE_BUILD_TYPE=Debug -DCMAKE_SYSTEM_NAME=Linux ..
    make
}
)
