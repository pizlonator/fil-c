#!/bin/bash

set -e
set -x

# json-glib 1.10.6.  Introspection is left on (gobject-introspection is
# installed); docs need gi-docgen and man needs xsltproc, both absent.

rm -rf pizlonated-json-glib
tar -xf pizlonated-json-glib.tar.gz
cd pizlonated-json-glib
mkdir build
cd build
meson setup \
      --prefix=/usr \
      --buildtype=debugoptimized \
      -D tests=false \
      -D installed_tests=false \
      -D documentation=disabled \
      -D man=false \
      ..
ninja
ninja install
cd ../..
rm -rf pizlonated-json-glib
hash -r
