#!/bin/bash

set -e
set -x

ulimit -c unlimited

# Moved here (postlc7, the GNOME desktop stage) from postlc11 -- gnome-control-center (postlc7) links libnm + libnma-gtk4 and gnome-shell (postlc8) loads the NMA4 typelib, so the whole WiFi userspace stack must be installed BEFORE g-c-c runs; postlc11 keeps only the kernel rebuild, the firmware and the boot wiring.
# libnl-3 3.11.0 -- the netlink library
# wpa_supplicant's nl80211 driver is built against (CONFIG_LIBNL32=y uses
# pkg-config libnl-3.0).  NetworkManager does NOT need it (NM has its own
# netlink stack in src/libnm-platform), so libnl exists here solely for the
# supplicant.
#
# Classic autotools.  Flags:
# --disable-static: house pattern (no .a files on the target).
# --disable-cli: libnl's nl-* command line tools need flex and install the
#   libnl-cli-3 library + cli plugins; nothing in the WiFi stack uses them
#   (wpa_supplicant needs only libnl-3 + libnl-genl-3 + libnl-route-3), so
#   the CLI tree is compiled out instead of adding flex to the chroot.
# --sysconfdir=/etc: libnl's /etc/libnl{,-3}/pct? config dir (classid cache)
#   lands in /etc where the target keeps its config.
#
# The make is parallel through the MAKEFLAGS=-j$(nproc) that
# build_chroot_late.sh exports.

cd /sources
rm -rf pizlonated-libnl
tar -xf pizlonated-libnl.tar.gz
cd pizlonated-libnl
./configure --prefix=/usr          \
            --sysconfdir=/etc      \
            --disable-static       \
            --disable-cli
make
make install
cd ..

# --- gates ---------------------------------------------------------------------

echo "=== gate: pkg-config modules ==="
pkg-config --modversion libnl-3.0 libnl-genl-3.0 libnl-route-3.0
test "x`pkg-config --modversion libnl-3.0`" = "x3.11.0"
test "x`pkg-config --modversion libnl-genl-3.0`" = "x3.11.0"
test "x`pkg-config --modversion libnl-route-3.0`" = "x3.11.0"

echo "=== gate: the libraries wpa_supplicant links ==="
test -e /usr/lib/libnl-3.so
test -e /usr/lib/libnl-genl-3.so
test -e /usr/lib/libnl-route-3.so
ls -la /usr/lib/libnl-3.so* /usr/lib/libnl-genl-3.so* /usr/lib/libnl-route-3.so*

echo "=== gate: the .pc files ==="
for pc in libnl-3.0.pc libnl-genl-3.0.pc libnl-route-3.0.pc; do
    test -e /usr/lib/pkgconfig/$pc
done
ls -la /usr/lib/pkgconfig/libnl*.pc

echo "=== gate: ldd of every library (no 'not found') ==="
for f in /usr/lib/libnl-3.so /usr/lib/libnl-genl-3.so /usr/lib/libnl-route-3.so; do
    if ldd $f 2>&1 | grep -q 'not found'; then
        echo "FATAL: $f has unresolved shared libraries:" >&2
        ldd $f 2>&1 | grep 'not found' >&2
        exit 1
    fi
done
echo "ldd: clean"

echo "=== gate: header wpa_supplicant's Makefile includes ==="
test -e /usr/include/libnl3/netlink/genl/genl.h
test -e /usr/include/libnl3/netlink/genl/ctrl.h
test -e /usr/include/libnl3/netlink/route/link.h

rm -rf pizlonated-libnl
hash -r

echo "postlc7 libnl: ALL GATES GREEN"
