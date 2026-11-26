#!/bin/bash

set -e
set -x

rm -rf pizlonated-libdrm
tar -xf pizlonated-libdrm.tar.gz
cd pizlonated-libdrm
mkdir build
cd build
meson setup --prefix=/usr         \
            --buildtype=debugoptimized \
            -D udev=true          \
            -D valgrind=disabled  \
            ..
ninja
ninja install
cd ../..
rm -rf pizlonated-libdrm
hash -r
