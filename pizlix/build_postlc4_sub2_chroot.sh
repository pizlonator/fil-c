#!/bin/bash

set -e
set -x

ulimit -c unlimited

cd /sources

tar -xf fribidi-1.0.15.tar.xz
cd fribidi-1.0.15
mkdir -v build
cd build
meson setup --prefix=/usr --buildtype=debugoptimized
ninja
ninja install
cd ../..
rm -rf fribidi-1.0.15
hash -r

tar -xf shared-mime-info-2.4.tar.gz
cd shared-mime-info-2.4
mkdir -v build
cd build
meson setup --prefix=/usr --buildtype=debugoptimized -D update-mimedb=true ..
ninja
ninja install
cd ../..
rm -rf shared-mime-info-2.4
hash -r
