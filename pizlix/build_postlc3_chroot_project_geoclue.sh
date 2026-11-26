#!/bin/bash

set -e
set -x

# geoclue 2.7.2 (D-Bus geolocation service + libgeoclue + Geoclue-2.0 gir
# for gnome-shell's location panel).
#
# Flags (checked against meson_options.txt):
# -D 3g-source=false -Dcdma-source=false -Dmodem-gps-source=false: all need
#   ModemManager (not ported).
# -D nmea-source=false: needs Avahi libraries (not present).
# -D demo-agent=false: demo GTK app, not needed.
# -D vapi=false: no valac in the chroot.
# -D gtk-doc=false: no docs.
# -D libgeoclue=true + introspection=true (defaults): Geoclue-2.0 typelib.
# -D enable-backend=true (default): build the geoclue service itself.
#   The service runs as the 'geoclue' user at runtime (dbus-srv-user default);
#   no build-time user check, but create the user anyway.
getent passwd geoclue >/dev/null || useradd -r -M -d / -s /bin/false geoclue

cd /sources
rm -rf pizlonated-geoclue
tar -xf pizlonated-geoclue.tar.gz
cd pizlonated-geoclue
mkdir -v filc-build
cd filc-build
meson setup ..                  \
      --prefix=/usr             \
      --buildtype=debugoptimized \
      -D3g-source=false         \
      -Dcdma-source=false       \
      -Dmodem-gps-source=false  \
      -Dnmea-source=false       \
      -Ddemo-agent=false        \
      -Dvapi=false              \
      -Dgtk-doc=false
ninja
ninja install
cd ../..
rm -rf pizlonated-geoclue
hash -r
