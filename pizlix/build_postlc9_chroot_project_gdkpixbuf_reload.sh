#!/bin/bash

set -e
set -x

# Sprint S4 of the librsvg-via-Fil-Rust port, support build: rebuild
# gdk-pixbuf with the queryloaders.c RTLD_LOCAL patch and reinstall it, then
# regenerate the loaders.cache.
#
# Why: gdk-pixbuf-query-loaders opened every loader module with
# g_module_open (path, 0) = RTLD_LAZY|RTLD_GLOBAL.  Loader modules all export
# the same entry-point names ("fill_info"/"fill_vtable"), and the Fil-C dlsym
# resolves within the global scope in load order rather than preferring the
# given handle, so the cache writer associated every module path after the
# first with the first module's format info; gdk_pixbuf_new_from_file then
# could not recognize any format handled by a dlopened loader (SVG included).
# The patch loads modules RTLD_LOCAL (matching gdk-pixbuf-io.c's runtime
# loader), which makes dlsym resolve within the handle's own module.  See
# the Fil-C port comment in queryloaders.c.

rm -rf /sources/pizlonated-gdk-pixbuf
tar -xf /sources/pizlonated-gdk-pixbuf.tar.gz -C /sources
cd /sources/pizlonated-gdk-pixbuf

export PKG_CONFIG_PATH=/usr/lib/pkgconfig:/usr/share/pkgconfig

meson setup build \
      --prefix=/usr             \
      --buildtype=debugoptimized \
      -D others=enabled          \
      --wrap-mode=nofallback     \
      -D man=false               \
      -D gtk_doc=false           \
      -D tests=false
ninja -C build
ninja -C build install

# Regenerate the module cache with the patched query-loaders.
/usr/bin/gdk-pixbuf-query-loaders --update-cache

cd ..
rm -rf pizlonated-gdk-pixbuf

echo "gdk-pixbuf queryloaders RTLD_LOCAL rebuild: OK"
