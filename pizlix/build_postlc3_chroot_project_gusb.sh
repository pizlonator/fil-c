#!/bin/bash

set -e
set -x

# libgusb 0.4.2.  Needs libusb-1.0 (built just before) and json-glib.
# docs need gi-docgen (absent), vapi needs vapigen (absent), umockdev is
# absent; introspection stays on (gobject-introspection is installed).

rm -rf pizlonated-gusb
tar -xf pizlonated-gusb.tar.gz
cd pizlonated-gusb
mkdir build
cd build
meson setup \
      --prefix=/usr \
      --buildtype=debugoptimized \
      -D docs=false \
      -D tests=false \
      -D vapi=false \
      -D umockdev=disabled \
      ..
ninja
ninja install
cd ../..
rm -rf pizlonated-gusb
hash -r
