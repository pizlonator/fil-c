#!/bin/bash

set -e
set -x

# gnome-font-viewer 46.0 (the GNOME font viewer app).
#
# Flags: none beyond the standard prefix/buildtype (the only project option
# is 'profile', left at 'default').
#
# Dependencies from this stage: libadwaita-1; plus pre-existing gtk4,
# harfbuzz, fontconfig, freetype2, gnome-desktop-4, fribidi.
#
# Fil-C notes: none needed; the sources have no hand-rolled g_once_init
# statics, GType switches, or GType OR-flags, and the project does not pin
# GLIB_VERSION_MAX_ALLOWED below 2.80 (the pizlonated glib needs the
# 2.80 G_DEFINE_TYPE path).

# NB: cd before rm (same as the gjs/gnome-shell scripts): make the relative
# paths below independent of the chroot's inherited CWD.
cd /sources
rm -rf pizlonated-gnome-font-viewer
tar -xf pizlonated-gnome-font-viewer.tar.gz
cd pizlonated-gnome-font-viewer
mkdir -v build
cd build
# llvmpipe-on-JIT (sprint 3): GTK4's GSK GL renderer loads swrast_dri.so and
# runs llvmpipe shader compilation in-process, so this executable must carry
# the JIT-enabled Fil-C CRT: -enable-filc-jit at LINK time swaps
# filc_jit_crt.o in.  The flag is ignored (with a warning) for -shared links.
# meson only reads LDFLAGS at setup.
export LDFLAGS="-enable-filc-jit"
meson setup ..                   \
      --prefix=/usr              \
      --buildtype=debugoptimized
ninja
ninja install
cd ../..
rm -rf pizlonated-gnome-font-viewer
hash -r