#!/bin/bash -e

portdir=$(cd $(dirname "$0") && pwd);
projdir=${portdir}/..

libzip_commit_id="efaef47e5b7d08de4223704dd65ba9fecc5e5818"
libxml2_commit_id="bdec2183f34b37ee89ae1d330c6ad2bb4d76605f"
json_commit_id="acbcc062f9c114f7b2a63b792897fdfffed71d14"

#
# Build libzip library
#
build_libzip()
{
    if [ "$1" = "clean" ]; then
        rm -rf ${projdir}/libzip-build
        return 0
    fi
    
    if [ ! -d ${projdir}/libzip-build ]; then
        cp -rf ${projdir}/libzip ${projdir}/libzip-build
        cd ${projdir}/libzip-build
        git apply ${portdir}/deps/libzip.patch
    fi

    echo "Building libzip library..."

    cd ${projdir}/libzip-build

    if [ "$(git rev-parse HEAD)" != "$libzip_commit_id" ]; then
        echo "Wrong commit id, expected $libzip_commit_id"
    fi

    if [ ! -d build ]; then
        mkdir build && cd build
        cmake -DCMAKE_BUILD_TYPE=Release \
              -DCMAKE_TOOLCHAIN_FILE=${portdir}/cmake/${toolchain} \
              -DBUILD_SHARED_LIBS:BOOL=ON ..
    else
        cd build
    fi

    make -j
}

#
# Build json-c library
#
build_json_c()
{
    if [ "$1" = "clean" ]; then
        rm -rf ${projdir}/json-c-build
        return 0
    fi
    
    if [ ! -d ${projdir}/json-c-build ]; then
        cp -rf ${projdir}/json-c ${projdir}/json-c-build
    fi

    echo "Building json-c library..."

    cd ${projdir}/json-c-build

    if [ "$(git rev-parse HEAD)" != "$json_commit_id" ]; then
        echo "Wrong commit id, expected $json_commit_id"
    fi

    autoreconf -f -i
    ./configure --host=${am_host} ac_cv_func_malloc_0_nonnull=yes ac_cv_func_realloc_0_nonnull=yes --enable-static --disable-shared

    if [ "$XL4_TARGET_OS" = "qnx660" ]; then
        make -j CFLAGS='-std=gnu99 -fPIC -fvisibility=hidden -Wno-implicit-fallthrough' CPPFLAGS='-include $(PWD)/../json-c-rename.h'
    else
        make -j CFLAGS='-fPIC -fvisibility=hidden -Wno-implicit-fallthrough' CPPFLAGS='-include $(PWD)/../json-c-rename.h'
    fi
}

#
# Build libxml2 library
#
build_libxml2()
{
    if [ "$1" = "clean" ]; then
        rm -rf ${projdir}/libxml2-build
        return 0
    fi
    
    if [ ! -d ${projdir}/libxml2-build ]; then
        cp -rf ${projdir}/libxml2 ${projdir}/libxml2-build
    fi

    echo "Building libxml2 library..."

    cd ${projdir}/libxml2-build

    if [ "$(git rev-parse HEAD)" != "$libxml2_commit_id" ]; then
        echo "Wrong commit id, expected $libxml2_commit_id"
    fi

    autoreconf -f -i
    ./configure --host=${am_host} --without-python

    if [ "$XL4_TARGET_OS" = "qnx660" ]; then
        make -j CFLAGS='-std=gnu99'
    else
        make -j
    fi
}

#
# Build all libraries
#
build_all()
{
    build_libzip $1
    build_json_c $1
    
    if [ "$XL4_TARGET_OS" = "qnx660" ]; then
        build_libxml2 $1
    fi
}

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
# Process user input
#
case "$1" in
    ("all") 
        build_all $2;;
    ("libzip") 
        build_libzip $2 ;;
    ("libxml2") 
        build_libxml2 $2 ;;
    ("json-c") 
        build_json_c $2 ;;
    *) 
echo "Unknown option
Usage: "$0" <target> [clean]
Supported <target> values:
  all
  libzip
  libxml2
  json-c"
esac
