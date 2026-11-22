#!/bin/bash

set -e
set -x

ulimit -c unlimited

# Sprint C of the WiFi-on-Pizlix port: moved here (postlc7, the GNOME desktop stage) from postlc11 -- gnome-control-center (postlc7) links libnm + libnma-gtk4 and gnome-shell (postlc8) loads the NMA4 typelib, so the whole WiFi userspace stack must be installed BEFORE g-c-c runs; postlc11 keeps only the kernel rebuild, the firmware and the boot wiring.
# Sprint B of the WiFi-on-Pizlix port: libndp 1.9 -- the IPv6 Neighbor
# Discovery Protocol library.  NetworkManager 1.48.4 declares it as an
# UNCONDITIONAL dependency (meson.build:257 `libndp_dep =
# dependency('libndp')` -- used by the ndp manager in src/core/platform for
# IPv6 neighbor discovery on shared/dhcpv6 connections).
#
# The Debian pool .orig tarball is the upstream git tag archive: it ships
# configure.ac + autogen.sh but NO generated configure, so this script runs
# autoreconf first (autoconf 2.72 / automake 1.17 / libtool 2.4.7 are in the
# chroot -- verified).  autogen.sh itself is exactly `autoreconf --force
# --install -I m4`; we inline that (and keep the build-aux aux dir it
# generates) instead of invoking the script, which also tries to run
# ./configure afterwards in some layouts.
#
# Flags:
# --disable-static: house pattern.  There is no --disable-bin option; the
#   package's one binary, ndptool (a small ndp diagnostic, the closest thing
#   the target will ever have to iw), is kept -- it installs to /usr/bin and
#   doubles as a runtime sanity check of the library.

cd /sources
rm -rf pizlonated-libndp
tar -xf pizlonated-libndp.tar.gz
cd pizlonated-libndp
autoreconf --force --install -I m4
./configure --prefix=/usr     \
            --sysconfdir=/etc \
            --disable-static
make
make install
cd ..

# --- gates ---------------------------------------------------------------------

echo "=== gate: pkg-config ==="
pkg-config --modversion libndp
test "x`pkg-config --modversion libndp`" = "x1.9"

echo "=== gate: library + .pc ==="
test -e /usr/lib/libndp.so
test -e /usr/lib/pkgconfig/libndp.pc
ls -la /usr/lib/libndp.so* /usr/lib/pkgconfig/libndp.pc

echo "=== gate: header ==="
test -e /usr/include/ndp.h

echo "=== gate: ndptool runs under Fil-C (help path) ==="
/usr/bin/ndptool --help 2>&1 | head -3

echo "=== gate: ldd of the library (no 'not found') ==="
if ldd /usr/lib/libndp.so 2>&1 | grep -q 'not found'; then
    echo "FATAL: libndp.so has unresolved shared libraries:" >&2
    ldd /usr/lib/libndp.so 2>&1 | grep 'not found' >&2
    exit 1
fi
echo "ldd: clean"

rm -rf pizlonated-libndp
hash -r

echo "postlc7 libndp: ALL GATES GREEN"
