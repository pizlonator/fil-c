#!/bin/bash

set -e
set -x

# gnome-text-editor 46.3 (the GNOME text editor app).
#
# Flags:
# -D enchant=enabled: spellchecking via enchant (built in an earlier stage).
#
# Dependencies from this stage: gtksourceview-5, libadwaita-1,
# editorconfig (all built earlier in this stage), plus pre-existing gtk4,
# cairo, enchant.
#
# Fil-C notes (see the projeny patch): GLIB_VERSION_MAX_ALLOWED is pinned
# to GLIB_VERSION_2_80 so that G_DEFINE_TYPE uses the pointer-based
# g_once_init_enter_pointer path required by the pizlonated glib.

# NB: cd before rm (same as the gjs/gnome-shell scripts): make the relative
# paths below independent of the chroot's inherited CWD.
cd /sources
rm -rf pizlonated-gnome-text-editor
tar -xf pizlonated-gnome-text-editor.tar.gz
cd pizlonated-gnome-text-editor
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
      --buildtype=debugoptimized \
      -D enchant=enabled
ninja
ninja install
cd ../..
rm -rf pizlonated-gnome-text-editor
hash -r