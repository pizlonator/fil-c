#!/bin/bash

set -e
set -x

# gdk-pixbuf 2.42.12.  The checked-in source carries the Fil-C queryloaders
# RTLD_LOCAL fix, so this FIRST build produces a correct loaders.cache: with
# the stock queryloaders.c, gdk-pixbuf-query-loaders opened every loader
# module with g_module_open (path, 0) = RTLD_LAZY|RTLD_GLOBAL; loader
# modules all export the same entry-point names ("fill_info"/"fill_vtable"),
# and the Fil-C dlsym resolves within the global scope in load order rather
# than preferring the given handle, so the cache writer associated every
# module path after the first with the first module's format info;
# gdk_pixbuf_new_from_file then could not recognize any format handled by a
# dlopened loader (SVG included).  The patch loads modules RTLD_LOCAL
# (matching gdk-pixbuf-io.c's runtime loader), which makes dlsym resolve
# within the handle's own module -- see the Fil-C port comment in
# queryloaders.c.  (The old layout re-ran this build plus a cache regen in a
# later stage because that stage's snapshot predated the fix; the single
# build here is self-sufficient.)

rm -rf pizlonated-gdk-pixbuf
tar -xf pizlonated-gdk-pixbuf.tar.gz
cd pizlonated-gdk-pixbuf
mkdir -v build
cd build
meson setup .. --prefix=/usr --buildtype=debugoptimized -D others=enabled --wrap-mode=nofallback -D man=false -D gtk_doc=false -D tests=false
ninja
ninja install
cd ../..
rm -rf pizlonated-gdk-pixbuf

# Regenerate the module cache with the patched query-loaders this build just
# installed.
/usr/bin/gdk-pixbuf-query-loaders --update-cache
