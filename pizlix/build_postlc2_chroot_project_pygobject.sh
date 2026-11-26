#!/bin/bash

set -e
set -x

rm -rf pizlonated-pygobject
tar -xf pizlonated-pygobject.tar.gz
cd pizlonated-pygobject
mkdir -v build
cd build
meson setup --prefix=/usr --buildtype=debugoptimized ..
ninja
ninja install
cd ../..
rm -rf pizlonated-pygobject
