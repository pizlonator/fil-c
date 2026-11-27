#!/bin/bash

set -e
set -x

ulimit -c unlimited

test "x$FILCSRC" != "x"
test -d $FILCSRC
test -d $FILCSRC/projects

test $EUID -eq `stat -c %u $FILCSRC`

cd $FILCSRC

# Packaging for the merged postlc2 stage (the old postlc2..postlc5 stages'
# sub1 lists, unioned in build order; identical lines deduped).  Everything
# the stage's chroot script extracts is packaged here.

rm -vf projects/*/pizlonated-*.tar.gz

# CLI/network/crypto tools (the old postlc2 stage).
./package-source.sh projects/cmake-3.30.2 pizlonated-cmake
./package-source.sh projects/libtasn1-4.21.0 pizlonated-libtasn1
filc/projeny package projects/p11-kit.projeny projects/p11-kit/pizlonated-p11-kit.tar.gz
filc/projeny package projects/libidn2.projeny projects/libidn2/pizlonated-libidn2.tar.gz
filc/projeny package projects/libunistring.projeny projects/libunistring/pizlonated-libunistring.tar.gz
filc/projeny package projects/curl.projeny projects/curl/pizlonated-curl.tar.gz
./package-source.sh projects/emacs-30.1 pizlonated-emacs
./package-source.sh projects/sudo-1.9.17p2 pizlonated-sudo
filc/projeny package projects/git.projeny projects/git/pizlonated-git.tar.gz
filc/projeny package projects/libuv.projeny projects/libuv/pizlonated-libuv.tar.gz
filc/projeny package projects/pcre2.projeny projects/pcre2/pizlonated-pcre2.tar.gz
filc/projeny package projects/libxml2.projeny projects/libxml2/pizlonated-libxml2.tar.gz
filc/projeny package projects/icu.projeny projects/icu/pizlonated-icu.tar.gz
filc/projeny package projects/brotli.projeny projects/brotli/pizlonated-brotli.tar.gz
filc/projeny package projects/nghttp2.projeny projects/nghttp2/pizlonated-nghttp2.tar.gz
./package-source.sh projects/libarchive-3.7.4 pizlonated-libarchive
./package-source.sh projects/dhcpcd-10.0.8 pizlonated-dhcpcd
filc/projeny package projects/openssh.projeny projects/openssh/pizlonated-openssh.tar.gz
./package-source.sh projects/yaml-0.2.5 pizlonated-yaml
./package-source.sh projects/ruby-3.3.10 pizlonated-ruby
filc/projeny package projects/patchelf.projeny projects/patchelf/pizlonated-patchelf.tar.gz

# Wayland/Weston + text-rendering foundation (the old postlc3 stage).
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
./package-source.sh projects/libjpeg-turbo-3.1.4 pizlonated-libjpeg-turbo
if test "$(uname -m)" = x86_64
then
    filc/projeny setup projects/simde.projeny
    filc/projeny package projects/simde.projeny projects/simde/pizlonated-simde.tar.gz
fi
./package-source.sh projects/tiff-4.6.0 pizlonated-tiff
./package-source.sh projects/libwebp-1.4.0 pizlonated-libwebp
./package-source.sh projects/openjpeg-2.5.2 pizlonated-openjpeg
./package-source.sh projects/gobject-introspection-1.80.1 pizlonated-gobject-introspection
./package-source.sh projects/freetype-2.13.3 pizlonated-freetype
./package-source.sh projects/fontconfig-2.15.0 pizlonated-fontconfig
./package-source.sh projects/harfbuzz-9.0.0 pizlonated-harfbuzz
./package-source.sh projects/graphite2-1.3.14 pizlonated-graphite2

# lcms2 2.16, from its projeny package (the Fil-C package).  Built ONCE in
# this stage's chroot; no later stage re-packages or rebuilds it (the plain
# lcms2-2.16.tar.gz build of the old layout is gone).
filc/projeny package projects/lcms2.projeny projects/lcms2/pizlonated-lcms2.tar.gz

# GTK/GL foundation (the old postlc4 stage).
./package-source.sh projects/pygobject-3.48.2 pizlonated-pygobject
./package-source.sh projects/graphene-1.10.8 pizlonated-graphene
filc/projeny package projects/mesa.projeny projects/mesa/pizlonated-mesa.tar.gz
./package-source.sh projects/pango-1.54.0 pizlonated-pango
./package-source.sh projects/gdk-pixbuf-2.42.12 pizlonated-gdk-pixbuf
./package-source.sh projects/at-spi2-core-2.60.5 pizlonated-at-spi2-core
./package-source.sh projects/gtk-3.24.52 pizlonated-gtk3
./package-source.sh projects/gtk-4.14.5 pizlonated-gtk4
./package-source.sh projects/nettle-3.10 pizlonated-nettle
./package-source.sh projects/libgudev-238 pizlonated-libgudev
./package-source.sh projects/gstreamer-1.24.7 pizlonated-gstreamer
./package-source.sh projects/gst-plugins-bad-1.24.7 pizlonated-gst-plugins-bad
./package-source.sh projects/gst-plugins-base-1.24.7 pizlonated-gst-plugins-base

# Crypto/text libraries (the old postlc5 stage).
./package-source.sh projects/gnutls-3.8.7.1 pizlonated-gnutls
./package-source.sh projects/glib-networking-2.80.0 pizlonated-glib-networking
./package-source.sh projects/libsoup-3.4.4 pizlonated-libsoup
./package-source.sh projects/libgpg-error-1.50 pizlonated-libgpg-error
./package-source.sh projects/libgcrypt-1.11.0 pizlonated-libgcrypt
./package-source.sh projects/aspell-0.60.8.1 pizlonated-aspell
./package-source.sh projects/enchant-2.8.2 pizlonated-enchant
