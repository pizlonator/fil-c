#!/bin/bash

set -e
set -x

# gnome-menus 3.36.0 (freedesktop .desktop menu layout + libgnome-menu-3 +
# GMenu-3.0 gir; gnome-shell's app grid and app-favorites eat this).
#
# Autotools build (3.36.0 never moved to meson).  Introspection is on by
# default when gobject-introspection is found (GOBJECT_INTROSPECTION_CHECK) —
# we need the GMenu-3.0 typelib.  gettext is AM_GNU_GETTEXT external.
#
# Fil-C fixes (projects/gnome-menus.projeny):
# - libmenu/desktop-entries.c get_current_desktops(): the static gchar** was
#   round-tripped through gsize by the plain g_once_init_enter/leave — use
#   the _pointer variants so the pointer keeps its object metadata.
cd /sources
rm -rf pizlonated-gnome-menus
tar -xf pizlonated-gnome-menus.tar.gz
cd pizlonated-gnome-menus
./configure          \
    --prefix=/usr    \
    --sysconfdir=/etc \
    --localstatedir=/var \
    --disable-static
make
make install
cd ../..
rm -rf pizlonated-gnome-menus
hash -r
