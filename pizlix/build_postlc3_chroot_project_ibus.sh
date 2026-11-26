#!/bin/bash

set -e
set -x

# Built in postlc3: gnome-control-center (later in this
# stage) hard-requires ibus-1.0 (the ibus option is left enabled);
# gnome-shell (also this stage, after g-c-c) needs the IBus-1.0 typelib
# too.  Its own hard deps
# are all earlier-or-in-stage: glib/gio (postlc2), dbus-1 (the dbus build
# at the top of this stage), iso-codes (postlc2).

# ibus 1.5.30 (input-method framework; gnome-shell imports IBus-1.0 for its
# keyboard/input-method integration).
#
# Flags (./configure --help):
# --enable-introspection=yes: IBus-1.0 gir + libibus-1.0.
# --disable-gtk2 --disable-gtk3: no gtk2/3 im modules (gtk4 module off by
#   default; gnome-shell talks to ibus over D-Bus, not through an im module).
# --disable-xim: XIM server not needed (wayland).
# --disable-ui --disable-setup: no gtk-based setup UI.
# --disable-engine: skip the simple engine (daemon-side extra).
# --disable-dconf: skip dconf-based config persistence (dconf is installed,
#   but keep ibus minimal; gnome-shell does not need it).
# --disable-systemd-services: no systemd in the chroot (elogind).
# --disable-python2: implies --disable-python-library (no python ibus lib).
# --disable-emoji-dict --disable-unicode-dict: need unicode-data/iso-codes.
# --disable-libnotify: not ported.
# --disable-tests; --enable-vala=no (no valac).
#
# Fil-C notes: no version script in ibus (all symbols default-visible, so the
# pizlonated_ aliases export fine); the GType-idiom scan found no
# switch-over-GType and no GType-typed g_once_init statics in src/ or bus/.
cd /sources
rm -rf pizlonated-ibus
tar -xf pizlonated-ibus.tar.gz
cd pizlonated-ibus
./configure                   \
    --prefix=/usr             \
    --sysconfdir=/etc         \
    --localstatedir=/var      \
    --enable-introspection=yes \
    --enable-vala=no          \
    --disable-gtk2            \
    --disable-gtk3            \
    --disable-gtk4            \
    --disable-xim             \
    --disable-ui              \
    --disable-setup           \
    --disable-engine          \
    --disable-dconf           \
    --disable-systemd-services \
    --disable-python2         \
    --disable-python-library  \
    --disable-emoji-dict      \
    --disable-unicode-dict    \
    --disable-libnotify       \
    --disable-appindicator    \
    --disable-tests           \
    --disable-static
make
make install
cd ../..
rm -rf pizlonated-ibus
hash -r
