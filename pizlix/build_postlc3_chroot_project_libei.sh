#!/bin/bash

set -e
set -x

# libei 1.5.0.  Needs an sd-bus provider: libelogind (installed by the
# elogind build earlier in this stage) is auto-detected.  Tests are
# disabled; documentation is an array option that defaults to [] (no docs),
# so it is left alone.  Provides libei-1.0 and libeis-1.0.

rm -rf pizlonated-libei
tar -xf pizlonated-libei.tar.gz
cd pizlonated-libei
mkdir build
cd build
meson setup \
      --prefix=/usr \
      --buildtype=debugoptimized \
      -D tests=disabled \
      ..
ninja
ninja install
cd ../..
rm -rf pizlonated-libei
hash -r
