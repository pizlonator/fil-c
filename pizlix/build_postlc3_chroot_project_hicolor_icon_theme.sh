#!/bin/bash

set -e
set -x

# hicolor-icon-theme 0.18 (the default fallback icon theme).
#
# Pure data: index.theme, the per-size/context empty icon-directory
# skeleton, and a dataonly pkg-config file (default-icon-theme.pc).  No
# compilation, so no Fil-C issues.  This fills the icon-theme lookup chain
# (gtk/gdk: requested theme -> hicolor fallback), which the Adwaita theme
# from Adwaita alone does not provide (Adwaita ships no hicolor index.theme).
#
# 0.18 is meson-based (0.18 switched from autotools to meson); the meson
# install step installs the data and creates the empty directories.  Unlike
# Adwaita it runs no icon-cache update (nothing to cache; the theme is
# empty dirs + index.theme).
#
# NB: cd before rm: the chroot drops us in the caller's CWD, so the rm must
# happen with an absolute CWD for the relative path to hit the stale tree
# (same as the gjs/gnome-shell scripts).
cd /sources
rm -rf pizlonated-hicolor-icon-theme
tar -xf pizlonated-hicolor-icon-theme.tar.gz
cd pizlonated-hicolor-icon-theme
mkdir -v build
cd build
meson setup ..                   \
      --prefix=/usr              \
      --buildtype=debugoptimized
ninja
ninja install
cd ../..
rm -rf pizlonated-hicolor-icon-theme

# The index.theme is the whole point; assert it landed.
test -f /usr/share/icons/hicolor/index.theme

hash -r
