#!/bin/bash -e

portdir=$(cd $(dirname "$0") && pwd);
projdir=${portdir}/..

#
# Check supported target os
#
if [ "$XL4_TARGET_OS" != "qnx660" ] && [ "$XL4_TARGET_OS" != "qnx700" ]; then
    echo "Unsupported target operating system ($XL4_TARGET_OS)"
    echo "Please export XL4_TARGET_OS to one of below values:"
    echo "    qnx660"
    echo "    qnx700"
    exit 1
fi

#
# Check supported architect
#
if [ "$XL4_TARGET_ARCH" != "armv7l" ] && [ "$XL4_TARGET_ARCH" != "aarch64l" ]; then
    echo "Unsupported target architecture ($XL4_TARGET_ARCH)"
    echo "Please export XL4_TARGET_ARCH to one of below values:"
    echo "    armv7l"
    echo "    aarch64l"
    exit 1
fi

#
# Check unsupported combination
#
if [ "$XL4_TARGET_OS" = "qnx660" ] && [ "$XL4_TARGET_ARCH" = "aarch64l" ]; then
    echo "Target aarch64l is not supported on qnx660"
    exit 1
fi

#
# Check qnx660 build environment
#
if [ "$XL4_TARGET_OS" = "qnx660" ] && [ "$XL4_TARGET_ARCH" = "armv7l" ]; then
    if [[ "$QNX_TARGET" != *"qnx6"* ]]; then
        echo "Please set qnx660 build environment (XL4_TARGET_OS=qnx660)"
        exit 1
    else
        am_host="arm-unknown-nto-qnx6.6.0eabi"
        toolchain="qnx660_armv7l.cmake"
    fi
fi

#
# Check qnx700 build environment
#
if [ "$XL4_TARGET_OS" = "qnx700" ]; then
    if [[ "$QNX_TARGET" != *"qnx7"* ]]; then
        echo "Please set qnx700 build environment (XL4_TARGET_OS=qnx700)"
        exit 1
    else
        if [ "$XL4_TARGET_ARCH" = "armv7l" ]; then
            am_host="arm-unknown-nto-qnx7.0.0eabi"
            toolchain="qnx700_armv7l.cmake"
        else
            am_host="aarch64-unknown-nto-qnx7.0.0"
            toolchain="qnx700_aarch64l.cmake"
        fi
    fi
fi

#
# Clean build directory
#
if [ "$1" = "clean" ]; then
    rm -rf ${projdir}/build
    exit 0
fi

#
# Build lib and program
#
cd ${projdir} && mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=${portdir}/cmake/${toolchain} ..
      
make -j

