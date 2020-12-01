#!/bin/sh -ex

portdir=`pwd`
rootdir=${portdir}/..

build_json_c () {

    cd ${rootdir}

    if [ ! -f "json-c/.git" ]; then
        git submodule update json-c
    fi

    if [ ! -d json-c-build ];then
        cp -fr json-c json-c-build
    fi

    cd json-c-build

    if [ ! -e .libs/libjson-c.a ]; then
        autoreconf -f -i

        CFLAGS="-fPIC -fvisibility=hidden -Wno-error" \
        CPPFLAGS="-include $(pwd)/../json-c-rename.h" \
        ./configure ${CONFIGURE_FLAGS} \
        --enable-static --disable-shared

        make -j
    fi

}

if [ "$1" = 'clean' ] ; then
    rm -rf json-c-build
    rm ${portdir}/build_deps.done
else
    git submodule init
    build_json_c
    touch ${portdir}/build_deps.done
fi

echo "Done!"

exit 0
