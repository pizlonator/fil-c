#!/bin/bash

set -e
set -x

# gnome-terminal 3.54.5 (the GNOME terminal application).
#
# Flags:
# -D docs=false: no yelp manual (yelp-tools are not part of the pizlonated
#   system; the man pages would also pull in xsltproc processing that we do
#   not need - the ui files still need xsltproc, which IS in the chroot).
# -D nautilus_extension=false: nautilus is not installed.
# -D search_provider=false: the gnome-shell search provider is dropped.
#
# Dependencies from this stage: vte-2.91 (built earlier in this stage); plus
# pre-existing gtk3, gio, pcre2, gsettings-desktop-schemas, libuuid, x11.
#
# Fil-C notes (see the projeny patch):
# - GLIB_VERSION_MAX_ALLOWED pinned to GLIB_VERSION_2_80 (gnome-terminal pins
#   2.68; same reasoning as vte).
# - libhandy is not part of the pizlonated system; it was only used for the
#   HdyStyleManager window-chrome color-scheme handling in terminal-app.cc,
#   which is replaced with the GtkSettings gtk-application-prefer-dark-theme
#   path (the pre-existing non-libhandy fallback).

# NB: cd before rm (same as the gjs/gnome-shell scripts): make the relative
# paths below independent of the chroot's inherited CWD.
cd /sources
rm -rf pizlonated-gnome-terminal
tar -xf pizlonated-gnome-terminal.tar.gz
cd pizlonated-gnome-terminal
mkdir -v build
cd build
meson setup ..                   \
      --prefix=/usr              \
      --buildtype=debugoptimized \
      -D docs=false              \
      -D nautilus_extension=false \
      -D search_provider=false
ninja
ninja install
cd ../..
rm -rf pizlonated-gnome-terminal
hash -r
