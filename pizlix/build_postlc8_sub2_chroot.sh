#!/bin/bash

set -e
set -x

ulimit -c unlimited

cd /sources

# The GNOME Shell stage.  The builds below were verified live on the
# running system; this stage wires the
# same builds into the chain idiomatically.  The two gir rebuilds (mutter,
# gnome-desktop) are drop-in same-version rebuilds over the postlc7
# installs, now with GObject introspection enabled, because gnome-shell's
# JS needs their typelibs.

# pygobject first: the Fil-C import-gi fix (the postlc4 build panicked on
# `python3 -c "import gi"`).  Everything below that runs g-ir-scanner or a
# python gi module at build time (libgweather's gen_locations_variant.py)
# depends on this rebuild.
./build_postlc8_chroot_project_pygobject.sh
hash -r

# mutter 46.9 rebuilt WITH introspection: Meta/Clutter/Cally/Cogl/Mtk-14
# typelibs.  gnome-shell needs all of them.  Drop-in same-version rebuild
# over the postlc7 install (uses the postlc7 pizlonated-mutter tarball).
./build_postlc8_chroot_project_mutter_gir.sh
hash -r

# gnome-desktop 44.5 rebuilt WITH introspection: GnomeDesktop-4.0 /
# GnomeBG-4.0 typelibs for the shell's clock and animated backgrounds.
# Drop-in same-version rebuild (uses the sub1-packaged tarball, which
# carries the postlc8 gnome-rr introspection Fil-C fixes; the copy in the
# recovered snapshot predates them).
./build_postlc8_chroot_project_gnome_desktop_gir.sh
hash -r

# at-spi2-core 2.60.5 full build (the postlc4 install was atk_only):
# libatspi + Atspi-2.0 gir/typelib + the ATK bridge.  Drop-in same-version
# rebuild over the atk_only install.
./build_postlc8_chroot_project_at_spi.sh
hash -r


# Location: geoclue first, then geocode-glib, then libgweather (its meson
# setup runs python3 'import gi', proving the pygobject import-gi fix
# above).
./build_postlc8_chroot_project_geoclue.sh
hash -r

./build_postlc8_chroot_project_geocode_glib.sh
hash -r

./build_postlc8_chroot_project_libgweather.sh
hash -r

./build_postlc8_chroot_project_gnome_menus.sh
hash -r

# gnome-autoar: gnome-shell's extensions-tool subproject uses it.
./build_postlc8_chroot_project_gnome_autoar.sh
hash -r

# hicolor-icon-theme: the default fallback icon theme (data only), which
# completes the icon-theme lookup chain for the session.
./build_postlc8_chroot_project_hicolor_icon_theme.sh
hash -r

# The Shell runtime, last: mozjs (the engine, ~10-15 min), then gjs (the
# bindings, which build against mozjs), then gnome-shell (which needs the
# mutter/gnome-desktop/at-spi typelibs from above plus gjs).
./build_postlc8_chroot_project_mozjs.sh
hash -r

./build_postlc8_chroot_project_gjs.sh
hash -r

# Deferred NM + libnma gates: part 7 must build NetworkManager and libnma
# before gnome-control-center links libnm + libnma-gtk4, but gjs is only
# built here in part 8 -- so the networkmanager/libnma scripts'
# "gjs can load the typelib" gates are deferred to this point, now that
# gjs exists.
gjs -c "const NM = imports.gi.NM; print('NM', NM.MAJOR_VERSION, NM.MINOR_VERSION);"
gjs -c "const NMA4 = imports.gi.NMA4; print('NMA4 loaded, WifiDialog ctor:', typeof NMA4.WifiDialog, NMA4.WifiDialog.\$gtype);"

./build_postlc8_chroot_project_gnome_shell.sh
hash -r
