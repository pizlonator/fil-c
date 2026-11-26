#!/bin/bash

set -e
set -x

rm -rf pizlonated-atk
tar -xf pizlonated-atk.tar.gz
cd pizlonated-atk
mkdir -v build
cd build
meson setup .. --prefix=/usr --buildtype=debugoptimized -D introspection=false
ninja
ninja install
cd ../..
rm -rf pizlonated-atk
