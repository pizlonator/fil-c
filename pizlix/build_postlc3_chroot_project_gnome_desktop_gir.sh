#!/bin/bash

set -e
set -x

# gnome-desktop 44.5 rebuild WITH GObject introspection (drop-in same-version
# rebuild, same pattern as build_postlc3_chroot_project_mutter_gir.sh):
# gnome-shell's JS needs the GnomeDesktop-4.0 and GnomeBG-4.0 typelibs
# (js/misc/dependencies.js requires them; GnomeDesktop.WallClock drives the
# panel clock, GnomeBG.BGSlideShow drives animated backgrounds).  The first
# build earlier in this stage had introspection off.  Everything else is
# identical; the installed gnome-desktop stays 44.5.  (The tarball this
# re-extracts is the same pizlonated-gnome-desktop staged at postlc2, whose
# projeny patch carries the gnome-rr introspection Fil-C fixes.)
#
# Flags: identical to build_postlc3_chroot_project_gnome_desktop.sh, except:
# -D introspection=true: produce the GnomeDesktop-4.0 / GnomeBG-4.0 /
#   GnomeRR-4.0 girs and typelibs.  g-ir-scanner builds a small dumper
#   program against the just-built libs and runs it.
cd /sources
rm -rf pizlonated-gnome-desktop
tar -xf pizlonated-gnome-desktop.tar.gz
cd pizlonated-gnome-desktop
mkdir -v build
cd build
meson setup ..              \
      --prefix=/usr         \
      --buildtype=debugoptimized \
      -D systemd=disabled   \
      -D debug_tools=false  \
      -D introspection=true \
      -D desktop_docs=false
ninja
ninja install
cd ../..
rm -rf pizlonated-gnome-desktop
hash -r
