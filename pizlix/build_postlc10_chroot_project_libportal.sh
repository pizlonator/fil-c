#!/bin/bash

set -e
set -x

ulimit -c unlimited


cd /sources
rm -rf pizlonated-libportal
tar -xf pizlonated-libportal.tar.gz
cd pizlonated-libportal
mkdir -v build
cd build
meson setup ..                   \
      --prefix=/usr              \
      --buildtype=debugoptimized \
      -D tests=false             \
      -D vapi=false              \
      -D docs=false
ninja
ninja install
cd ../..

# --- gates ---------------------------------------------------------------------

echo "=== gate: pkg-config modules ==="
pkg-config --modversion libportal libportal-gtk3 libportal-gtk4
test "x`pkg-config --modversion libportal`" = "x0.9.1"
test "x`pkg-config --modversion libportal-gtk3`" = "x0.9.1"
test "x`pkg-config --modversion libportal-gtk4`" = "x0.9.1"

echo "=== gate: libraries installed ==="
test -e /usr/lib/libportal.so
test -e /usr/lib/libportal-gtk3.so
test -e /usr/lib/libportal-gtk4.so
ls -la /usr/lib/libportal*.so*

echo "=== gate: introspection data installed ==="
# Fil-C port note: libportal 0.9.1's gir namespace/version is Xdp-1.0 (the
# introspection version is 1.0 even though the package version is 0.9.1 --
# the gir/typelib names are Xdp-1.0, XdpGtk3-1.0, XdpGtk4-1.0).
test -e /usr/share/gir-1.0/Xdp-1.0.gir
test -e /usr/share/gir-1.0/XdpGtk3-1.0.gir
test -e /usr/share/gir-1.0/XdpGtk4-1.0.gir
test -e /usr/lib/girepository-1.0/Xdp-1.0.typelib
ls -la /usr/share/gir-1.0/Xdp*.gir /usr/lib/girepository-1.0/Xdp*.typelib

echo "=== gate: ldd of every library (no 'not found') ==="
for f in /usr/lib/libportal.so /usr/lib/libportal-gtk3.so /usr/lib/libportal-gtk4.so; do
    if ldd $f 2>&1 | grep -q 'not found'; then
        echo "FATAL: $f has unresolved shared libraries:" >&2
        ldd $f 2>&1 | grep 'not found' >&2
        exit 1
    fi
done
echo "ldd: clean"

rm -rf pizlonated-libportal
hash -r

echo "postlc10 libportal: ALL GATES GREEN"
