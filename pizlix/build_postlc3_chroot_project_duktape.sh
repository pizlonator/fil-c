#!/bin/bash

set -e
set -x


# duktape 2.7.0 (embeddable JS engine) — required by polkit 126, whose JS rule
# authority (polkitbackendduktapeauthority.c) links against a SYSTEM duktape
# (polkit 0.121+ dropped the bundled copy; the polkit-126 meson.build wants
# duktape >= 2.2.0 via pkg-config or duktape.h + -lduktape).
#
# Build with the bundled Makefile.sharedlibrary: one amalgamated duktape.c
# compiled -shared, headers + duktape.pc installed.  INSTALL_PREFIX=/usr with
# LIBDIR=/lib puts libduktape.so in /usr/lib and duktape.pc (libdir=/usr/lib)
# in /usr/lib/pkgconfig.
#
# Fil-C notes: on x86-64 duk_config.h selects the unpacked duk_tval layout
# (pointers never smuggled through doubles), no inline asm, no computed
# gotos — it builds as plain C.  No version script (duktape exports its API
# through the default visibility; no .map file ships).
cd /sources
rm -rf pizlonated-duktape
tar -xf pizlonated-duktape.tar.gz
cd pizlonated-duktape
make -f Makefile.sharedlibrary INSTALL_PREFIX=/usr LIBDIR=/lib libduktape.so.207.20700 duktape.pc
make -f Makefile.sharedlibrary INSTALL_PREFIX=/usr LIBDIR=/lib install
cd ../..
rm -rf pizlonated-duktape
hash -r
