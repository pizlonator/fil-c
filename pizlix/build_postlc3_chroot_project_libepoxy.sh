#!/bin/bash
set -e
set -x
rm -rf libepoxy-1.5.10
tar -xf libepoxy-1.5.10.tar.xz
cd libepoxy-1.5.10
mkdir -v build
cd build
meson setup .. --prefix=/usr --buildtype=debugoptimized -Dglx=yes -Degl=yes -Dx11=true -Dtests=false -Ddocs=false
ninja
ninja install
cd ../..
rm -rf libepoxy-1.5.10
