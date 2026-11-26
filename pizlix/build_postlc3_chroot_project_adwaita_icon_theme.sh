#!/bin/bash

set -e
set -x

# adwaita-icon-theme 46.2 (data-only: icons, cursors, index.theme, and a
# dataonly pkg-config file).
#
# No compilation; the meson install step runs gtk4-update-icon-cache to
# (re)generate /usr/share/icons/Adwaita/icon-theme.cache.
#
# NOTE (documented limitation): the Adwaita icons are SVG.  They will not
# render at runtime until librsvg is ported and registered as a gdk-pixbuf
# loader; installing the theme now is still what the apps expect
# (icon-theme.cache, index.theme, icon paths).

rm -rf pizlonated-adwaita-icon-theme
tar -xf pizlonated-adwaita-icon-theme.tar.gz
cd pizlonated-adwaita-icon-theme
mkdir -v build
cd build
meson setup ..                   \
      --prefix=/usr              \
      --buildtype=debugoptimized
ninja
ninja install
cd ../..
rm -rf pizlonated-adwaita-icon-theme
hash -r