#!/bin/bash -e
git submodule init
git submodule update

#DBG="-g -O0"

# Build json-c:
rm -rf json-c-build
cp -fr json-c json-c-build
pushd json-c-build
autoreconf -f -i
CFLAGS="-fPIC -fvisibility=hidden -Wno-implicit-fallthrough" \
CPPFLAGS="-include $(pwd)/../json-c-rename.h" \
./configure --enable-static --disable-shared
make -j
popd

rm -rf build
mkdir build
pushd  build
cmake ..
make -j
popd
