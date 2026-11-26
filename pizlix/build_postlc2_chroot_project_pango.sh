#!/bin/bash

set -e
set -x

rm -rf pizlonated-pango
tar -xf pizlonated-pango.tar.gz
cd pizlonated-pango
mkdir -v build
cd build
meson setup .. --prefix=/usr --buildtype=debugoptimized --wrap-mode=nofallback
ninja
ninja install
cd ../..
rm -rf pizlonated-pango
