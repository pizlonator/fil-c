#!/bin/bash
set -e
set -x
rm -rf pizlonated-cairo
tar -xf pizlonated-cairo.tar.gz
cd pizlonated-cairo
mkdir -v build
cd build
meson setup .. --prefix=/usr --buildtype=debugoptimized -Dxlib=enabled -Dxcb=disabled -Dxlib-xcb=disabled -Dtests=disabled -Dgtk2-utils=disabled -Dgtk_doc=false
ninja
ninja install
cd ../..
rm -rf pizlonated-cairo
