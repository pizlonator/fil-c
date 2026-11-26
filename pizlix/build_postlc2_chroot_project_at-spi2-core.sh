#!/bin/bash

set -e
set -x

rm -rf pizlonated-at-spi2-core
tar -xf pizlonated-at-spi2-core.tar.gz
cd pizlonated-at-spi2-core
mkdir -v build
cd build
meson setup .. --prefix=/usr --buildtype=debugoptimized -D atk_only=true -D x11=disabled -D introspection=enabled
ninja
ninja install
cd ../..
rm -rf pizlonated-at-spi2-core
