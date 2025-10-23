#!/bin/sh -eu

cd "$1"; shift
MESON_INSTALL_DESTDIR=${MESON_INSTALL_DESTDIR_PREFIX%$MESON_INSTALL_PREFIX}
dest="$MESON_INSTALL_DESTDIR$1"; shift

install -p -m644 -t "$dest" -- *.html
