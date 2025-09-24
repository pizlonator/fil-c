#!/bin/bash

set -e
set -x

rm -rf pizlonated-cairo
tar -xf pizlonated-cairo.tar.gz
cd pizlonated-cairo
mkdir build
cd build
meson setup --prefix=/usr --buildtype=debugoptimized ..
ninja
ninja install
cd ../..
rm -rf pizlonated-cairo
