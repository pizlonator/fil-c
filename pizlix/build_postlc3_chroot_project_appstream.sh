#!/bin/bash

set -e
set -x

# appstream 1.0.3.  Everything optional is turned off: stemming needs
# libstemmer, systemd unit files need systemd, compose/svg support pull in
# extra deps, qt is not built, docs need tooling that is absent, and the
# gir is not needed by anything in this port.  zstd support stays on
# (libzstd is installed).  Needs libxmlb (built just before), curl and
# libxml2.

rm -rf pizlonated-appstream
tar -xf pizlonated-appstream.tar.gz
cd pizlonated-appstream
mkdir build
cd build
meson setup \
      --prefix=/usr \
      --buildtype=debugoptimized \
      -D stemming=false \
      -D systemd=false \
      -D qt=false \
      -D compose=false \
      -D apt-support=false \
      -D svg-support=false \
      -D docs=false \
      -D apidocs=false \
      -D install-docs=false \
      -D vapi=false \
      -D gir=false \
      ..
ninja
ninja install
cd ../..
rm -rf pizlonated-appstream
hash -r
