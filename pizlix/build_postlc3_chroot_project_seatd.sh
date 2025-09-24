#!/bin/bash

set -e
set -x

rm -rf pizlonated-seatd
tar -xf pizlonated-seatd.tar.gz
cd pizlonated-seatd
mkdir build
cd build
meson setup \
      --prefix=/usr \
      --buildtype=debugoptimized \
      -D libseat-logind=disabled \
      -D libseat-seatd=enabled \
      -D libseat-builtin=enabled \
      ..
ninja
ninja install
cd ../..
rm -rf pizlonated-seatd
