#!/bin/bash

CWD=$(pwd)
LIB_UA_DIR="${CWD}/.."

export PORT=ghs_port

git submodule init
git submodule update

XZ="${CWD}/xz"
ESDIFF="${CWD}/esdiff"
ZLIB="${CWD}/zlib"
LIBZIP="${CWD}/libzip"
LIBXML2="${CWD}/libxml2"

build_lib()
{
    build_dir="$1/build"

    [ ! -d "${build_dir}" ] && mkdir -p $build_dir
    cd $build_dir
    cmake .. && make
}

dirs=($XZ $ESDIFF $ZLIB $LIBZIP $LIBXML2)
for dir in "${dirs[@]}"; do
    build_lib $dir
done

# Build libua tmpl-updateaagent and deltapacher.bin
cd $LIB_UA_DIR
rm -rf build
mkdir build && cd build
cmake ..
make
cd $CWD
