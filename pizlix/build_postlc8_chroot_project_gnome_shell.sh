#!/bin/bash

set -e
set -x

# gnome-shell 46.9 (the GNOME 46 Shell; pinned to 46.9 to match the mutter
# 46.9 already in the tree).
#
# This is the Fil-C port.  The pizlonated tarball is packaged from
# projects/gnome-shell.projeny, which carries the port fixes (see the projeny
# patch for the full list; at packaging time this includes at least):
#
# - src/calendar-server is dropped: it hard-depends on libecal/libedataserver
#   (evolution-data-server), which we do not build.  The top-level meson.build
#   makes those deps optional and src/meson.build skips the subdir.  The JS
#   calendar panel tolerates the absence of the org.gnome.Shell.CalendarServer
#   D-Bus name (it just never shows events).
# - js/misc/dependencies.js: Gdm and Rsvg are runtime-optional (like
#   Malcontent).  Gdm (js/gdm/*, js/ui/unlockDialog.js, js/ui/screenShield.js,
#   js/misc/systemActions.js) and Rsvg (js/ui/padOsd.js) degrade gracefully;
#   screen locking is refused when Gdm is absent (otherwise the lock screen
#   would lock the user out, since it authenticates through Gdm).
#
# Flags (checked against meson_options.txt):
# -D networkmanager=false: no NetworkManager (the js NM status/agent code is
#   then compiled out via Config.HAVE_NETWORKMANAGER).
# -D camera_monitor=false: no pipewire (the camera monitor is the only
#   pipewire user in the shell).
# -D systemd=false: no libsystemd/systemd unit dirs (elogind provides logind).
# -D tests=false: no test harness (needs python-dbusmock etc.).
# -D man=false: skip the man page machinery (a2x not in the system; the
#   release tarball ships a prebuilt man/gnome-shell.1 but man=false skips it
#   entirely, which is fine).
# -D gtk_doc=false is already the default.
# -D extensions_app=false: the Extensions GUI app needs blueprint-compiler,
#   which we do not have.
# -D extensions_tool=true (default): builds the gnome-extensions CLI
#   (extensions-tool subproject) against the gnome-autoar package built
#   earlier in this batch.  It also installs the gnome-shell-extension-tool
#   wrapper script from src/.
#
# The theme: the release tarball ships prebuilt data/theme/*.css, so sassc is
# not needed (the meson build only runs sassc when the css is missing); we
# still put /yolo/bin (which has sassc, same trick as the libadwaita build)
# on the PATH as a fallback.  NOTE: /usr/bin must stay FIRST — the pizlonated
# gcc has to win over the yolo gcc.
export PATH=/usr/bin:/usr/sbin:/yolo/bin

# NB: cd before rm: the chroot drops us in the caller's CWD, so the rm must
# happen with an absolute CWD for the relative path to hit the stale tree
# (same as the gjs script).
cd /sources
rm -rf pizlonated-gnome-shell
tar -xf pizlonated-gnome-shell.tar.gz
cd pizlonated-gnome-shell
mkdir build
cd build
# llvmpipe-on-JIT (sprint 3): gnome-shell loads swrast_dri.so (via libmutter)
# and runs llvmpipe shader compilation, so the executable must carry the
# JIT-enabled Fil-C CRT: -enable-filc-jit at LINK time swaps filc_jit_crt.o
# in.  The flag is ignored (with a warning) for -shared links, so exporting it
# here is safe for the library link steps too.  meson only reads LDFLAGS at
# setup.
export LDFLAGS="-enable-filc-jit"
meson setup --prefix=/usr --buildtype=debugoptimized \
  -Dnetworkmanager=false -Dcamera_monitor=false -Dsystemd=false \
  -Dtests=false -Dman=false -Dgtk_doc=false \
  -Dextensions_app=false \
  ..
ninja
ninja install
cd ../..
rm -rf pizlonated-gnome-shell
hash -r
