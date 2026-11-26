#!/bin/bash

set -e
set -x

# at-spi2-core 2.60.5 full build (the gir path): the
# postlc2 stage built it with -D atk_only=true (just the ATK stub library for
# gtk3/gtk4).  gnome-shell needs the real thing: libatspi (Atspi-2.0 gir +
# typelib for the shell's a11y D-Bus traffic) and the ATK bridge
# (atk-bridge-2.0.pc, the GTK a11y module).  This is a drop-in same-version
# rebuild on top of the atk_only install.
#
# Flags:
# -D atk_only=false: build atspi + atk + the bridge, not just the ATK stub.
# -D x11=disabled: no X11 backend for the device event controller; the shell
#   talks to at-spi over D-Bus (the chroot's X11 client libs stay untouched).
# -D introspection=enabled: produce Atspi-2.0 gir/typelib via g-ir-scanner.
# -D gtk2_atk_adaptor=false: we do not have (and do not want) GTK2.
# -D default_bus=dbus-daemon: the chroot has the classic dbus (dbus-daemon),
#   not dbus-broker; this picks which launcher at-spi2 installs for the
#   accessibility bus.  use_systemd is for dbus-broker only, and we run
#   elogind, so leave it at its default and let dbus' autodetection win.
# -D docs=false is the default (no gtk-doc in the chroot).
# -D dbus_glib stays disabled: dbus-glib is dead and nothing needs it.
rm -rf pizlonated-at-spi2-core
tar -xf pizlonated-at-spi2-core.tar.gz
cd pizlonated-at-spi2-core
mkdir -v build
cd build
meson setup .. --prefix=/usr --buildtype=debugoptimized \
      -D atk_only=false         \
      -D x11=disabled           \
      -D introspection=enabled  \
      -D gtk2_atk_adaptor=false \
      -D default_bus=dbus-daemon \
      -D docs=false
ninja
ninja install
cd ../..
rm -rf pizlonated-at-spi2-core
hash -r
