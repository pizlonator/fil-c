#!/bin/bash

set -e
set -x

# gnome-control-center 46.8 (the GNOME Settings application).
#
# Flags:
# -D tests=false: the interactive tests need a display and python test deps.
# -D ibus left enabled: ibus 1.5.x is built in the chroot (the keyboard
#   panel's input-source list uses it).
# -D snap=false: no snapd.
# -D malcontent=false (the default): no malcontent.
# -D location-services=disabled: geoclue's location toggles are dropped with
#   no geoclue agent wired into the session.
#
# Panels kept: applications, background, display, keyboard, mouse,
# multitasking, notifications, power, search, sharing, universal-access.
# Panels dropped (their external dependencies are not part of the pizlonated
# system - see the projeny patch): color (colord-gtk4), online-accounts
# (goa-1.0), printers (cups/smbclient), privacy (libgtop), sound (gsound),
# system (libgtop/udisks2/libpwquality/krb5), network+wwan (libnm/libnma/
# mm-glib), bluetooth (gnome-bluetooth-ui-3.0), wacom (libwacom).
#
# Dependencies: gtk4, libadwaita-1, gnome-desktop-4/gnome-bg-4/gnome-rr-4,
# gsettings-desktop-schemas, polkit-gobject-1, accountsservice, colord,
# upower-glib, gudev, libpulse(+mainloop-glib, for the bundled gvc
# subproject), x11/xi, epoxy, gcr-4, ibus-1.0, libxml2 - all in the chroot
# by the time this runs (the audio/policy stack -- libpulse via
# libsndfile+pulseaudio, duktape+polkit, gcr, ibus, accountsservice,
# upower -- is built earlier in this stage, see
# build_postlc7_sub2_chroot.sh; it used to be a postlc8 thing, which
# broke from-scratch builds because the vendored gvc subproject executes
# BEFORE every other dependency check and dies on libpulse).
# The bundled gvc (gnome-volume-control) subproject builds with alsa=false.
#
# Fil-C notes (see the projeny patch):
# - gnome-settings-daemon is not built here; its gsd-enums.h (the only
#   external header the kept panels include) is vendored under
#   panels/common/gsd-include/gnome-settings-daemon/gsd-enums.h and exposed
#   through a declare_dependency.
# - tecla is referenced through the installed binary path instead of the
#   network git-clone subproject (the keyboard panel works without it).
# - cc-object-storage.c's pointer-typed once-init static uses
#   g_once_init_enter_pointer/leave_pointer.
# - GTK4 app: needs the JIT-enabled Fil-C CRT (see below).

# NB: cd before rm (same as the gjs/gnome-shell scripts): make the relative
# paths below independent of the chroot's inherited CWD.
cd /sources
rm -rf pizlonated-gnome-control-center
tar -xf pizlonated-gnome-control-center.tar.gz
cd pizlonated-gnome-control-center
mkdir -v build
cd build
# llvmpipe-on-JIT (sprint 3): GTK4's GSK GL renderer loads swrast_dri.so and
# runs llvmpipe shader compilation in-process, so this executable must carry
# the JIT-enabled Fil-C CRT: -enable-filc-jit at LINK time swaps
# filc_jit_crt.o in.  The flag is ignored (with a warning) for -shared links.
# meson only reads LDFLAGS at setup.
export LDFLAGS="-enable-filc-jit"
meson setup ..                   \
      --prefix=/usr              \
      --buildtype=debugoptimized \
      -D tests=false             \
      -D snap=false              \
      -D location-services=disabled
ninja
ninja install
cd ../..
rm -rf pizlonated-gnome-control-center
hash -r
