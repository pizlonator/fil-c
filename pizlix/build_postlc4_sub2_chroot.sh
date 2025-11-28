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

tar -xf pycairo-1.26.1.tar.gz
cd pycairo-1.26.1
mkdir -v build
cd build
meson setup --prefix=/usr --buildtype=debugoptimized ..
ninja
ninja install
cd ../..
rm -rf pycairo-1.26.1
hash -r

./build_postlc4_chroot_project_pygobject.sh
./build_postlc4_chroot_project_graphene.sh
