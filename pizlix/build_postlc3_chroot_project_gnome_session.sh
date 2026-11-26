#!/bin/bash

set -e
set -x

# gnome-session 46.0 (the session manager).
#
# Flags:
# -D systemduserunitdir=/usr/lib/systemd/user: install the user units, and
#   skip the `systemd` pkg-config lookup (only needed to autodetect the
#   unit dir; the units are inert without systemd, they are for a future
#   systemd-managed system).
# -D docbook=false: no docbook docs (needs xmlto).
# -D man=false: no man pages (needs xmlto).
#
# Fil-C notes (see the projeny patch): libsystemd is made optional with a
# fallback to libelogind (the pizlonated system uses elogind, whose
# libelogind exports the LIBSYSTEMD_209 sd_* API).  The `<systemd/*.h>`
# includes keep resolving because elogind installs a `systemd/` include
# dir under /usr/include/elogind and libelogind.pc points Cflags there.

rm -rf pizlonated-gnome-session
tar -xf pizlonated-gnome-session.tar.gz
cd pizlonated-gnome-session
mkdir -v build
cd build
meson setup ..                       \
      --prefix=/usr                  \
      --buildtype=debugoptimized     \
      -D systemduserunitdir=/usr/lib/systemd/user \
      -D docbook=false               \
      -D man=false
ninja
ninja install
cd ../..
rm -rf pizlonated-gnome-session
hash -r