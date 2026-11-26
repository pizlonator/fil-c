#!/bin/bash

set -e
set -x

# libxmlb 0.3.21.  gtkdoc and tests are off; introspection stays on and
# lzma/zstd support is auto-detected from liblzma/libzstd.

rm -rf pizlonated-libxmlb
tar -xf pizlonated-libxmlb.tar.gz
cd pizlonated-libxmlb
mkdir build
cd build
meson setup \
      --prefix=/usr \
      --buildtype=debugoptimized \
      -D gtkdoc=false \
      -D tests=false \
      ..
ninja
ninja install
cd ../..
rm -rf pizlonated-libxmlb
hash -r
