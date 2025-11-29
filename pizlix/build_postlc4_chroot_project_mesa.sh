#!/bin/bash

set -e
set -x

rm -rf pizlonated-mesa
tar -xf pizlonated-mesa.tar.gz
cd pizlonated-mesa
mkdir -v build
cd build
meson setup ..                 \
      --prefix=/usr            \
      --buildtype=debugoptimized \
      -D platforms=wayland     \
      -D gallium-drivers=swrast \
      -D vulkan-drivers=       \
      -D valgrind=disabled     \
      -D libunwind=disabled    \
      -D glx=disabled
ninja
ninja install
cd ../..
rm -rf pizlonated-mesa
