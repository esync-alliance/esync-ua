#!/bin/sh

set -e

TCR=${HOME}/tmp/linaro

if test -n "$USE_ARM_TOOLCHAIN"; then
    TCR="$USE_ARM_TOOLCHAIN"
fi

TCH=arm-linux-gnueabi
TCP="${TCR}/bin/${TCH}-"
export CPP=${TCP}cpp
export AR=${TCP}ar
export AS=${TCP}as
export NM=${TCP}nm
export CC=${TCP}gcc 
export CXX=${TCP}g++
export LD=${TCP}ld
export RANLIB=${TCP}ranlib
export MAKEFLAGS=-j
export SYSROOT="${TCR}/sysroot"
export USR="${SYSROOT}/usr"
export PKG_CONFIG_PATH=${USR}/lib/pkgconfig

mkdir -p arm_src
cd arm_src

#** zlib

if test ! -f zlib.ok; then
    rm -rf zlib
    git clone https://github.com/madler/zlib.git
    cd zlib
    ./configure --prefix=$USR
    make install
    cd ..
    touch zlib.ok
fi

#** libzip

if test ! -f libzip.ok; then
    rm -rf libzip
    git clone https://github.com/nih-at/libzip.git
    cd libzip
    git checkout rel-1-1-2
    autoreconf -f -i
    ./configure --prefix=$USR --host=$TCH --with-zlib=$USR
    make install
    cd ..
    touch libzip.ok
fi

#** openssl

if test ! -f openssl.ok; then
    rm -rf openssl
    git clone https://github.com/openssl/openssl.git
    cd openssl
    git checkout OpenSSL_1_0_2k
    ./config --prefix=$USR
    ./Configure dist -fPIC --prefix=$USR
    make -j1 install
    cd ..
    touch openssl.ok
fi

#** ares

if test ! -f cares.ok; then
    rm -rf c-ares
    git clone https://github.com/c-ares/c-ares.git
    cd c-ares
    autoreconf -f -i
    ./configure --prefix=$USR --host=$TCH
    make install
    cd ..
    touch cares.ok
fi

#** jansson

if test ! -f jansson.ok; then
    rm -rf jansson
    git clone https://github.com/akheron/jansson.git
    cd jansson
    autoreconf -f -i
    ./configure --prefix=$USR --host=$TCH
    make install
    cd ..
    touch jansson.ok
fi

#** cjose

if test ! -f cjose.ok; then
    rm -rf cjose
    git clone https://github.com/cisco/cjose.git
    cd cjose
    autoreconf -f -i
    ./configure --prefix=$USR --host=$TCH --with-openssl=$USR --with-jansson=$USR
    make install
    cd ..
    touch cjose.ok
fi

#** json-c

if test ! -f jsonc.ok; then
    rm -rf json-c
    git clone https://github.com/json-c/json-c.git
    cd json-c
    git checkout json-c-0.12-20140410
    autoreconf -f -i
    CFLAGS="-fPIC -fvisibility=hidden -Wno-error" CPPFLAGS="-include $(pwd)/../../json-c-rename.h" ./configure --prefix=$USR --host=$TCH --enable-static --disable-shared
    # AC_FUNC_MALLOC fails
    ed config.status < ../../cross_compile_scripts/jsonced
    make
    cd ..
    touch jsonc.ok
    cp -fr json-c ../json-c-build
fi

