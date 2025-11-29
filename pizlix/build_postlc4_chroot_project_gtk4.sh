#!/bin/bash

set -e
set -x

rm -rf pizlonated-gtk4
tar -xf pizlonated-gtk4.tar.gz
cd pizlonated-gtk4
mkdir -v build
cd build
meson setup .. --prefix=/usr --buildtype=debugoptimized -D broadway-backend=true -D introspection=enabled -D vulkan=disabled -D x11-backend=false -D media-gstreamer=disabled
ninja
ninja install
cd ../..
rm -rf pizlonated-gtk4
