#!/bin/bash

set -e
set -x

rm -rf pizlonated-glib
tar -xf pizlonated-glib.tar.gz
cd pizlonated-glib
mkdir build
cd build
meson setup ..                  \
      --prefix=/usr             \
      --buildtype=debugoptimized \
      -D introspection=disabled \
      -D documentation=false    \
      -D man-pages=disabled
ninja
ninja install
cd ../..
rm -rf pizlonated-glib

