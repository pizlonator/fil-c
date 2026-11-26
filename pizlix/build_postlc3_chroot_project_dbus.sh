#!/bin/bash

set -e
set -x

rm -rf pizlonated-dbus
tar -xf pizlonated-dbus.tar.gz
cd pizlonated-dbus
mkdir build
cd build
meson setup \
      --prefix=/usr \
      --buildtype=debugoptimized \
      -D x11_autolaunch=disabled \
      -D modular_tests=disabled \
      -D intrusive_tests=false \
      -D installed_tests=false \
      -D doxygen_docs=disabled \
      -D xml_docs=disabled \
      -D ducktype_docs=disabled \
      -D qt_help=disabled \
      -D selinux=disabled \
      -D apparmor=disabled \
      -D libaudit=disabled \
      -D systemd=disabled \
      -D valgrind=disabled \
      ..
ninja
ninja install
cd ../..
rm -rf pizlonated-dbus
