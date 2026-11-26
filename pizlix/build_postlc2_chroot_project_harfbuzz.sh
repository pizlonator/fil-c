#!/bin/bash

set -e
set -x

rm -rf pizlonated-harfbuzz
tar -xf pizlonated-harfbuzz.tar.gz
cd pizlonated-harfbuzz
mkdir build
cd build
meson setup ..             \
      --prefix=/usr        \
      --buildtype=debugoptimized  \
      -D graphite2=enabled
ninja
ninja install
cd ../..
rm -rf pizlonated-harfbuzz
