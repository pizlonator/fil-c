#!/bin/bash

set -e
set -x

rm -rf pizlonated-gtk3
tar -xf pizlonated-gtk3.tar.gz
cd pizlonated-gtk3
mkdir -v build
cd build
meson setup .. --prefix=/usr --buildtype=debugoptimized -D broadway_backend=true -D x11_backend=false -D wayland_backend=true --wrap-mode=nofallback
ninja
ninja install
cd ../..
rm -rf pizlonated-gtk3
