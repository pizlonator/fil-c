#!/bin/bash

set -e
set -x

rm -rf pizlonated-glib
tar -xf pizlonated-glib.tar.gz
cd pizlonated-glib
mkdir -v build
cd build
meson setup ..                  \
      --prefix=/usr             \
      --buildtype=debugoptimized \
      -D introspection=disabled \
      -D documentation=false    \
      -D man-pages=disabled
ninja
ninja install

../../build_postlc2_chroot_subproject_gobject_introspection.sh

# Now rebuild glib
meson configure -D introspection=enabled
ninja
ninja install

cd ../..
rm -rf pizlonated-glib

