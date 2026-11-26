#!/bin/bash

set -e
set -x

# colord 1.4.7, libraries only: the daemon is disabled (which also drops the
# polkit dependency), along with the systemd/udev/bash-completion extras and
# anything needing absent tooling (docs need gtk-doc tooling, man needs
# xsltproc, vapi needs vapigen).

rm -rf pizlonated-colord
tar -xf pizlonated-colord.tar.gz
cd pizlonated-colord
mkdir build
cd build
meson setup \
      --prefix=/usr \
      --buildtype=debugoptimized \
      -D daemon=false \
      -D systemd=false \
      -D udev_rules=false \
      -D bash_completion=false \
      -D tests=false \
      -D docs=false \
      -D man=false \
      -D argyllcms_sensor=false \
      -D vapi=false \
      -D introspection=false \
      -D libcolordcompat=false \
      -D print_profiles=false \
      -D sane=false \
      ..
ninja
ninja install
cd ../..
rm -rf pizlonated-colord
hash -r
