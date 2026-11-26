#!/bin/bash

set -e
set -x

# geocode-glib 3.26.4 (geocoding/reverse-geocoding library) — needed by
# libgweather 4.4.x (reverse geocode for the location search).
#
# SOUP DECISION (task item #8): NO soup3 port needed — upstream 3.26.4
# already grew libsoup3 support: meson has -Dsoup2 (default true) selecting
# libsoup-2.4 vs libsoup-3.0 (>= 2.99.2), library name geocode-glib-2,
# pkg geocode-glib-2.pc, gir GeocodeGlib-2.0; and every soup2-only call
# (soup_session_queue_message, soup_session_send_message, soup_uri_free...)
# is already guarded with #if SOUP_CHECK_VERSION (2, 99, 2) so that soup3
# headers select the soup_session_send_and_read_async paths.  So option (a)
# collapses to just passing -Dsoup2=false against the installed libsoup
# 3.4.4.  libgweather 4.4.x wants exactly this geocode-glib-2 API.
#
# Flags (checked against meson_options.txt):
# -D soup2=false: build against libsoup-3.0 (see above).
# -D enable-introspection=true (default): GeocodeGlib-2.0 gir for gjs.
# -D enable-gtk-doc=false -Denable-installed-tests=false.
#
# Fil-C fixes (projects/geocode-glib.projeny):
# - geocode-glib/geocode-glib.map: + pizlonated*; (before "local: *").
cd /sources
rm -rf pizlonated-geocode-glib
tar -xf pizlonated-geocode-glib.tar.gz
cd pizlonated-geocode-glib
mkdir -v filc-build
cd filc-build
meson setup ..                      \
      --prefix=/usr                 \
      --buildtype=debugoptimized    \
      -Dsoup2=false                 \
      -Denable-gtk-doc=false        \
      -Denable-installed-tests=false
ninja
ninja install
cd ../..
rm -rf pizlonated-geocode-glib
hash -r
