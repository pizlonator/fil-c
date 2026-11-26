#!/bin/bash

set -e
set -x

# gtksourceview 5.12.1 (the GtkSourceView 5 syntax highlighting engine for
# GTK4; gnome-text-editor needs it).
#
# Flags:
# -D build-testsuite=false: no test programs (they need xvfb-run).
# -D vapi=false: no vala.
# -D introspection=false: nothing in this stage needs the girs.
# -D documentation=false: no gi-docgen API reference.
# sysprof defaults to false.
#
# Fil-C notes: the pizlonated glib has pointer-valued GType, so this stage
# (see the projeny patch) switches the hand-rolled g_once_init_enter static
# in gtksourceutils.c to the _pointer variants and casts the
# `foo | G_SIGNAL_TYPE_STATIC_SCOPE` GType OR-flags to uintptr_t.

rm -rf pizlonated-gtksourceview
tar -xf pizlonated-gtksourceview.tar.gz
cd pizlonated-gtksourceview
mkdir -v build
cd build
meson setup ..                   \
      --prefix=/usr              \
      --buildtype=debugoptimized \
      -D build-testsuite=false   \
      -D vapi=false              \
      -D introspection=disabled   \
      -D documentation=false
ninja
ninja install
cd ../..
rm -rf pizlonated-gtksourceview
hash -r