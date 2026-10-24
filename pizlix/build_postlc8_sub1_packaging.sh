#!/bin/bash

set -e
set -x

ulimit -c unlimited

test "x$FILCSRC" != "x"
test -d $FILCSRC
test -d $FILCSRC/projects

test $EUID -eq `stat -c %u $FILCSRC`

cd $FILCSRC

rm -vf projects/*/pizlonated-*.tar.gz

# Sprint 6: assemble the postlc8 stage (GNOME Shell).  Everything the
# postlc8 chroot scripts extract is packaged here, except two drop-in
# same-version gir rebuilds that reuse the postlc7 tarballs already in
# /sources from the recovered snapshot (mutter, gnome-desktop; see below).
#
# All of the new packages are projeny projects (packaged with the projeny
# tool).  The one exception is at-spi2-core, which is not a projeny project:
# it is packaged from its checked-in source directory with package-source.sh
# (same pattern as postlc4's packaging, which produced the atk_only build).

# The Shell runtime: mozjs (the JS engine) and gjs (the JS bindings).
filc/projeny package projects/mozjs.projeny projects/mozjs/pizlonated-mozjs.tar.gz
filc/projeny package projects/gjs.projeny projects/gjs/pizlonated-gjs.tar.gz

# Session/policy services.
filc/projeny package projects/polkit.projeny projects/polkit/pizlonated-polkit.tar.gz
filc/projeny package projects/gcr.projeny projects/gcr/pizlonated-gcr.tar.gz
filc/projeny package projects/ibus.projeny projects/ibus/pizlonated-ibus.tar.gz
filc/projeny package projects/upower.projeny projects/upower/pizlonated-upower.tar.gz
filc/projeny package projects/accountsservice.projeny projects/accountsservice/pizlonated-accountsservice.tar.gz
filc/projeny package projects/geoclue.projeny projects/geoclue/pizlonated-geoclue.tar.gz

# Location stack (libgweather's meson setup runs python3 'import gi', which
# is why pygobject below is the Fil-C import-gi fix).
filc/projeny package projects/geocode-glib.projeny projects/geocode-glib/pizlonated-geocode-glib.tar.gz
filc/projeny package projects/libgweather.projeny projects/libgweather/pizlonated-libgweather.tar.gz

# Menus, audio, archive helpers.
filc/projeny package projects/gnome-menus.projeny projects/gnome-menus/pizlonated-gnome-menus.tar.gz
filc/projeny package projects/libsndfile.projeny projects/libsndfile/pizlonated-libsndfile.tar.gz
filc/projeny package projects/pulseaudio.projeny projects/pulseaudio/pizlonated-pulseaudio.tar.gz
filc/projeny package projects/gnome-autoar.projeny projects/gnome-autoar-0.4.4/pizlonated-gnome-autoar.tar.gz

# polkit 126's JS rule authority (system duktape; 0.116 bundled its own).
filc/projeny package projects/duktape.projeny projects/duktape/pizlonated-duktape.tar.gz

# pygobject: Sprint 3 brought the Sprint 4 build under projeny to carry the
# Fil-C import-gi fix (PyGObject_MetaType is still NULL while the _gi module
# executes; the pizlonated CPython asserts zhasvalidcap() in Py_SET_TYPE).
# Name is 'pygobject' (NOT 'pygobject-3.48.2'): the latter would collide with
# the checked-in projects/pygobject-3.48.2/ tree that postlc4's packaging
# uses (on a fresh clone the tracked tree exists but projeny's status file
# does not, so setup refuses).  The output NAME still matches what postlc4's
# packaging produced, so the chroot script and the cp glob are unchanged.
filc/projeny package projects/pygobject.projeny projects/pygobject/pizlonated-pygobject.tar.gz

# The Shell itself.
filc/projeny package projects/gnome-shell.projeny projects/gnome-shell/pizlonated-gnome-shell.tar.gz

# gnome-desktop must be REPACKAGED here even though the gir rebuild is a
# drop-in same-version rebuild: the postlc8 introspection fixes to
# gnome-rr (the gnome_rr_dpms_mode_get_type static-enum GType Fil-C fix and
# the gnome-rr gir meson wiring) were folded into projects/gnome-desktop.projeny
# AFTER the postlc7 stage was snapshotted, so the pizlonated-gnome-desktop
# tarball carried in the recovered snapshot predates them and the GnomeRR-4.0
# introspection dumper panics without them.  The cp below overwrites the
# stale copy in /sources.
filc/projeny package projects/gnome-desktop.projeny projects/gnome-desktop/pizlonated-gnome-desktop.tar.gz

# New in Sprint 6: hicolor-icon-theme (the default fallback icon theme;
# fixes the icon-theme lookup chain, which Adwaita alone does not provide).
filc/projeny package projects/hicolor-icon-theme.projeny projects/hicolor-icon-theme/pizlonated-hicolor-icon-theme.tar.gz

# at-spi2-core is not a projeny project (checked-in source dir; the same
# pattern as postlc4's packaging).  postlc8 rebuilds it WITH introspection
# (libatspi Atspi-2.0 gir/typelib + the ATK bridge) on top of postlc4's
# atk_only install.  The rm above deleted the postlc4 tarball, so repackage
# it here.
./package-source.sh projects/at-spi2-core-2.60.5 pizlonated-at-spi2-core

# NOT repackaged here (drop-in same-version gir rebuild reuses the postlc7
# tarball, which the recovered snapshot already has in /sources):
# - pizlonated-mutter        (build_postlc8_chroot_project_mutter_gir.sh)
# projects/mutter.projeny has not changed since the postlc7 packaging (and
# the mutter gir rebuild passed against the recovered tarball).

