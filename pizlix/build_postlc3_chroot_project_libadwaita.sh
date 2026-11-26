#!/bin/bash

set -e
set -x

# libadwaita 1.5.8 (the GTK4 design-language library used by all modern
# GNOME apps: gnome-text-editor, gnome-font-viewer, ...).
#
# Flags:
# -D tests=false: no unit tests.
# -D examples=false: no examples/demo (they would need extra deps).
# -D vapi=false: no vala.
# -D gtk_doc=false: no API reference.
# -D introspection=disabled: nothing in this stage needs the girs.
#
# Fil-C notes: none needed in the code; the library has no hand-rolled
# g_once_init statics, no GType switches, and its libadwaita.syms version
# script only exports adw_* which is exactly what its consumers link
# against (the pizlonated_* thunks are intra-library and stay local).
#
# Build notes: sassc (built in this stage under /yolo) compiles the
# stylesheet; the chroot PATH must include /yolo/bin or meson falls back to
# a sassc subproject wrap.  The compiled css is bundled into the library as
# a gresource.

rm -rf pizlonated-libadwaita
tar -xf pizlonated-libadwaita.tar.gz
cd pizlonated-libadwaita
export PATH=/usr/bin:/usr/sbin:/yolo/bin
mkdir -v build
cd build
meson setup ..                  \
      --prefix=/usr             \
      --buildtype=debugoptimized \
      -D tests=false            \
      -D examples=false         \
      -D vapi=false             \
      -D gtk_doc=false          \
      -D introspection=disabled
ninja
ninja install
cd ../..
rm -rf pizlonated-libadwaita
hash -r