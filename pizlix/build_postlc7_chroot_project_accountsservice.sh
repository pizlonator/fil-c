#!/bin/bash

set -e
set -x

# Built in postlc7 (moved from postlc8): gnome-control-center (later in this
# stage) hard-requires accountsservice >= 0.6.39 at meson setup; gnome-shell
# (postlc8) needs the AccountsService-1.0 typelib too.  Its own hard deps
# are all earlier-or-in-stage: polkit-gobject-1 (built just before this
# script -- accountsservice's meson.build requires it with no
# required:false), dbus-1 (the dbus build at the top of this stage),
# libelogind (-Delogind=true; the elogind build earlier in this stage),
# glib (postlc3).

# accountsservice 23.13.9 (D-Bus user-account service + AccountsService-1.0
# gir for gnome-shell's user menu).
#
# Flags (checked against meson_options.txt):
# -D elogind=true: use libelogind for sd-login (no libsystemd in the chroot).
# -D introspection=true (default): AccountsService-1.0 typelib needed.
# -D vapi=false: no valac in the chroot.
# -D systemdsystemunitdir=no: no systemd in the chroot (elogind); "no"
#   disables the systemd unit install (upstream requires systemd.pc otherwise).
# -D gtk_doc=false (default), -D docbook=false (default).
# -D tests=false: pizlix-added option (upstream 23.13.9 subdirs tests/
#   unconditionally, which drags in the mocklibc wrap subproject — that
#   subproject does not compile with strict C99 implicit-decl errors).
#
# Fil-C fixes (projects/accountsservice.projeny):
# - src/libaccountsservice/symbol.map: + pizlonated*;
#   (the generated enum/daemon code otherwise keeps every thunk local and the
#   g-ir-scanner dumper / daemon can't resolve the pizlonated_ aliases).
cd /sources
rm -rf pizlonated-accountsservice
tar -xf pizlonated-accountsservice.tar.gz
cd pizlonated-accountsservice
mkdir -v filc-build
cd filc-build
meson setup ..                   \
      --prefix=/usr              \
      --buildtype=debugoptimized \
      -Delogind=true             \
      -Dvapi=false               \
      -Dsystemdsystemunitdir=no  \
      -Dgtk_doc=false            \
      -Ddocbook=false            \
      -Dtests=false
ninja
ninja install
cd ../..
rm -rf pizlonated-accountsservice
hash -r
