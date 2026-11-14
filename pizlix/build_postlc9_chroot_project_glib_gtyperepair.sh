#!/bin/bash

set -e
set -x

# Sprint S3 of the librsvg-via-Fil-Rust port, support build: rebuild glib
# with the GType capability-repair patch and reinstall it, BEFORE the
# librsvg autotools build (whose g-ir-scanner run is the first exercise of
# Rust-registered GTypes).
#
# Why: for dynamic types a GType is the raw TypeNode pointer
# (type_node_any_new_W: type = GPOINTER_TO_TYPE (node)).  Plain C code
# carries a real filc pointer capability for it end to end, but the Fil-C
# rustc's Rust code sees glib-sys' "pub type GType = libc::size_t", i.e.
# bare integers: handing such a GType back over the FFI produces a
# pointer with no capability ("shadow"), and the first dereference of the
# decoded TypeNode panics the filc runtime ("cannot read pointer with
# null object").  Observed with glib-rs register_type ->
# g_type_add_instance_private, and with g-ir-scanner's dump program
# invoking rsvg_handle_get_type().  The patch (gobject/gtype.c) records
# every dynamic node in an address-keyed table at registration time and
# repairs lookups through it; see the Fil-C port comment in gtype.c.
#
# The original glib build (build_postlc3_chroot_project_glib.sh) had to
# build twice because gobject-introspection was not installed yet; it now
# is, so a single introspection=enabled pass reproduces the same install.
# ABI unchanged (same soname, no consumer rebuilds needed).
#
# -Dtests=false: with dbus now installed in the chroot, meson enables the
# gdbus/fdo test backends, whose upstream sources do `guint64/gint64 x =
 # <GType expr>` and trip the Fil-C clang's default-error int-conversion
# under this compiler (the postlc3 build predated dbus in the chroot and
# skipped those tests).  Tests are not installed, so they are skipped
# wholesale for this support rebuild.

rm -rf /sources/pizlonated-glib
tar -xf /sources/pizlonated-glib.tar.gz -C /sources
cd /sources/pizlonated-glib

export PKG_CONFIG_PATH=/usr/lib/pkgconfig:/usr/share/pkgconfig

meson setup build \
      --prefix=/usr             \
      --buildtype=debugoptimized \
      -D introspection=enabled \
      -D documentation=false    \
      -D man-pages=disabled     \
      -D tests=false
ninja -C build
ninja -C build install

cd ..
rm -rf pizlonated-glib

echo "glib GType-capability-repair rebuild: OK"
