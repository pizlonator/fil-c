#!/bin/bash

set -e
set -x

# libxcvt 0.1.3 (plain meson, no options).

rm -rf pizlonated-libxcvt
tar -xf pizlonated-libxcvt.tar.gz
cd pizlonated-libxcvt
mkdir build
cd build
meson setup \
      --prefix=/usr \
      --buildtype=debugoptimized \
      ..
ninja
ninja install
cd ../..
rm -rf pizlonated-libxcvt
hash -r
