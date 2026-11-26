#!/bin/bash

set -e
set -x

# Make sure we're in the glib build directory.
test -d gobject
test -d gmodule
test -d glib
test -e build.ninja
test -e meson-info

rm -rf pizlonated-gobject-introspection gi-build
tar -xf ../../pizlonated-gobject-introspection.tar.gz
meson setup pizlonated-gobject-introspection gi-build \
      --prefix=/usr --buildtype=debugoptimized
ninja -C gi-build
ninja -C gi-build install

