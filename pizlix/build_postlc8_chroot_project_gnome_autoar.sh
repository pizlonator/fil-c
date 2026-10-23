#!/bin/bash

set -e
set -x

# gnome-autoar 0.4.4 (archive extraction/compression helpers used by
# gnome-shell's extensions-tool subproject: the gnome-extensions CLI uses it
# to pack and install extension bundles).
#
# This is the Fil-C port.  The tarball is packaged from
# projects/gnome-autoar.projeny, which now points at the pristine upstream
# 0.4.4 tarball (projeny admits its one test fixture — a symlink to /tmp
# under tests/files/extract/test-symlink-parent/ — as a leaf symlink; the
# tests are not built, so it is never exercised).  No source changes were
# needed:
#
# - the custom mkenums template (autoar-enum-types.c.template) already uses a
#   plain static GType + `G_UNLIKELY (!type)` pattern, no g_once_init_enter,
#   so nothing to fix for Fil-C there.
# - the library is plain C against glib + libarchive (both already ported).
#
# Flags (checked against meson_options.txt):
# -D gtk=false: extensions-tool only uses the core libgnome-autoar; the GTK
#   widget helpers (libgnome-autoar-gtk) would drag in gtk3.
# -D tests=false: no test harness (the tests want the glib test framework
#   and exercise the extracted-files fixtures, incl. the /tmp symlink).
# -D vapi=false: no vala compiler in the system.
# -D gtk_doc=false is already the default.
# introspection stays auto: g-ir-scanner runs and installs Autoar-0, which is
# harmless and keeps the dumper path exercised.
cd /sources
rm -rf pizlonated-gnome-autoar
tar -xf pizlonated-gnome-autoar.tar.gz
cd pizlonated-gnome-autoar
mkdir -v build
cd build
meson setup ..               \
      --prefix=/usr          \
      --buildtype=debugoptimized \
      -Dgtk=false            \
      -Dtests=false          \
      -Dvapi=false           \
      -Dgtk_doc=false
ninja
ninja install
cd ../..
rm -rf pizlonated-gnome-autoar
hash -r
