#!/bin/bash

set -e
set -x

# gnome-desktop 44.5.
#
# Builds both libgnome-desktop-3 (legacy library, needed by gnome-session)
# and libgnome-desktop-4 (needed by mutter and gnome-font-viewer).
#
# Flags:
# -D systemd=disabled: no libsystemd; elogind provides the logind API.
# -D debug_tools=false: skips the gnome-datetime-source test tool.
# -D introspection=false: nothing in this stage needs the girs.
# -D desktop_docs=false: Yelp help documents (needs yelp tooling).
# udev is left at its default (auto): libudev.pc from elogind is found.
# gtk_doc defaults to false; installed_tests defaults to false.

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
      -D introspection=false \
      -D desktop_docs=false
ninja
ninja install
cd ../..
rm -rf pizlonated-gnome-desktop
hash -r
