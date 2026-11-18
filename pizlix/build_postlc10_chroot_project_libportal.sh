#!/bin/bash

set -e
set -x

ulimit -c unlimited

# Sprint W3 of the Epiphany-on-Pizlix port: libportal 0.9.1.
#
# The flatpak portal access library (XDG desktop portal client).  Epiphany
# 46 links libportal-gtk4 (meson dependency libportal-gtk4 >= 0.6) for the
# dynamic-launcher portal (web app install) and the open-uri portal.
#
# Backends (meson_options.txt: backend-gtk3/-gtk4/-qt5/-qt6, all 'auto'):
# gtk3 and gtk4 are in the chroot so both auto-enable (Epiphany needs the
# gtk4 one); qt5/qt6 auto-disable (no Qt in the chroot, and meson's qt5
# module import at the top of libportal's meson.build is lazy -- it does
# not probe for Qt tools until a Qt backend is enabled, so the import is
# safe without Qt).
#
# Flags:
# -D tests=false: the unit tests want a portal backend / mock dbus and the
#   build-time check_programs would pull extra bits; nothing needs them.
# -D vapi=false: no vala/vapigen in the chroot (house pattern, cf. gcr).
# -D docs=false: gi-docgen is not vendored in the chroot.
# introspection stays on (default true): g-ir-scanner + the Gtk-3.0/Gtk-4.0
# girs are installed, so XdpGtk3-2.0/XdpGtk4-2.0 girs + typelibs build --
# same house pattern as gcr-4 in postlc7.
#
# Fil-C notes: libportal has no version scripts (.map files) and no
# GLIB_VERSION_* pinning of its own (so GLIB_VERSION_MAX_ALLOWED defaults
# to the pizlonated glib's latest = 2.80, which routes the G_DEFINE_TYPE
# boilerplate through the pointer-based _g_type_once_init_* forms).  The
# two hand-rolled once-init pairs in libportal/portal.c are FIXED here by
# the projeny patch (see below); no other Fil-C changes were needed.

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
