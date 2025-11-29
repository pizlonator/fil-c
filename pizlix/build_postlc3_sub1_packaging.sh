#!/bin/bash

set -e
set -x

ulimit -c unlimited

test $EUID -ne 0

test "x$FILCSRC" != "x"
test -d $FILCSRC
test -d $FILCSRC/projects

cd $FILCSRC

rm -vf projects/*/pizlonated-*.tar.gz
./package-source.sh projects/wayland-1.24.0 pizlonated-wayland
./package-source.sh projects/libevdev-1.11.0 pizlonated-libevdev
./package-source.sh projects/libinput-1.29.1 pizlonated-libinput
./package-source.sh projects/libxkbcommon-xkbcommon-1.11.0 pizlonated-xkbcommon
./package-source.sh projects/libpng-1.6.43 pizlonated-libpng
./package-source.sh projects/glib-2.80.4 pizlonated-glib
./package-source.sh projects/cairo-1.18.0 pizlonated-cairo
./package-source.sh projects/weston-12.0.5 pizlonated-weston
./package-source.sh projects/seatd-0.9.1 pizlonated-seatd
./package-source.sh projects/libdrm-2.4.122 pizlonated-libdrm
./package-source.sh projects/libjpeg-turbo-3.0.1 pizlonated-libjpeg-turbo
./package-source.sh projects/tiff-4.6.0 pizlonated-tiff
./package-source.sh projects/libwebp-1.4.0 pizlonated-libwebp
./package-source.sh projects/openjpeg-2.5.2 pizlonated-openjpeg
./package-source.sh projects/gobject-introspection-1.80.1 pizlonated-gobject-introspection
./package-source.sh projects/freetype-2.13.3 pizlonated-freetype
./package-source.sh projects/fontconfig-2.15.0 pizlonated-fontconfig
./package-source.sh projects/harfbuzz-9.0.0 pizlonated-harfbuzz
