#!/bin/bash

set -e
set -x

# libgweather 4.4.4 (weather/location database + GWeather-4.0 gir for
# gnome-shell's weather quick-settings).
#
# Flags (checked against meson_options.txt):
# -D introspection=true (default): GWeather-4.0 typelib.
# -D tests=false: the tests want pygobject and the network; skip them
#   (postlc2's pygobject build carries the Fil-C import-gi fix, but the
#   tests also need network access, so they stay off).
# -D gtk_doc=false: gi-docgen wrap in subprojects/ — not vendored, not needed.
# -D enable_vala=false: no valac in the chroot ('auto' would skip it too).
# -D soup2=false (default): libsoup 3 in the chroot.
# -D owm_apikey default ''.
# geocode-glib-2.0 comes from the geocode-glib soup3 build (see that script).
cd /sources
rm -rf pizlonated-libgweather
tar -xf pizlonated-libgweather.tar.gz
cd pizlonated-libgweather
mkdir -v filc-build
cd filc-build
meson setup ..                  \
      --prefix=/usr             \
      --buildtype=debugoptimized \
      -Dtests=false             \
      -Dgtk_doc=false           \
      -Denable_vala=false
ninja
ninja install
cd ../..
rm -rf pizlonated-libgweather
hash -r
