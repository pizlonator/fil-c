#!/bin/bash

set -e
set -x

ulimit -c unlimited

# Built in postlc3 (the GNOME desktop stage), ordered before g-c-c -- gnome-control-center (later in this stage) links libnm + libnma-gtk4 and gnome-shell (the shell block, after g-c-c) loads the NMA4 typelib, so the whole WiFi userspace stack must be installed BEFORE g-c-c runs; the kernel rebuild, the firmware and the boot wiring run at the very end of this stage.
# wpa_supplicant 2.11 -- the WiFi
# authentication daemon that NetworkManager D-Bus-activates on demand
# (NM's src/core/supplicant/nm-supplicant-manager.c calls the
# fi.w1.wpa_supplicant1 D-Bus service; there is no init script and none is
# needed -- dbus activation + NM's own respawn handle the lifecycle).
#
# The projeny tarball is the whole hostap archive (wpa_supplicant-2.11/
# containing hostapd/, wpa_supplicant/, src/ shared tree); we build only
# the wpa_supplicant/ subdirectory.
#
# Config: defconfig already carries the NM-relevant options (CONFIG_DRIVER_
# NL80211=y L32, CONFIG_LIBNL32=y L47, CONFIG_IEEE8021X_EAPOL=y L94,
# CONFIG_CTRL_IFACE=y, CONFIG_CTRL_IFACE_DBUS_NEW=y L378 and
# CONFIG_CTRL_IFACE_DBUS_INTRO=y L381 -- NOTE the option is named
# ..._INTRO, not ..._INTROSPECTION), and CONFIG_TLS defaults to openssl in
# the Makefile when unset.  The four options are appended anyway so the
# .config is self-documenting and immune to defconfig drift (duplicate
# lines are harmless: the last occurrence wins).
#
# make BINDIR=/usr/sbin LIBDIR=/usr/lib: BINDIR is not just where `make
# install` puts the binaries -- the Makefile's %.service rule substitutes
# @BINDIR@ into dbus/fi.w1.wpa_supplicant1.service, so the generated
# D-Bus activation file must be produced with BINDIR=/usr/sbin for its
# Exec= line to point at the real binary path.
#
# Install is manual (the Makefile install target only handles BINALL into
# BINDIR + the optional libpasn/libwpa_client shared objects; the dbus
# service + policy files have no install rule -- BLFS pattern):
#   /usr/sbin/wpa_supplicant          the daemon
#   /usr/bin/wpa_cli                  the control client (user-facing)
#   /usr/bin/wpa_passphrase           PSK generator
#   /usr/share/dbus-1/system-services/fi.w1.wpa_supplicant1.service
#                                     the dbus activation file
#   /usr/share/dbus-1/system.d/wpa_supplicant.conf
#                                     the dbus security policy
#   /etc/wpa_supplicant/              config home (NM drives everything
#                                     via dbus; the file is a stub)

cd /sources
rm -rf pizlonated-wpa_supplicant
tar -xf pizlonated-wpa_supplicant.tar.gz
cd pizlonated-wpa_supplicant/wpa_supplicant

cp defconfig .config
cat >> .config << "EOF"
# --- the NM wiring (see the header comment) ---
CONFIG_CTRL_IFACE_DBUS_NEW=y
CONFIG_CTRL_IFACE_DBUS_INTRO=y
CONFIG_DRIVER_NL80211=y
CONFIG_LIBNL32=y
EOF

make BINDIR=/usr/sbin LIBDIR=/usr/lib

# --- install -----------------------------------------------------------------

install -m755 -D wpa_supplicant /usr/sbin/wpa_supplicant
install -m755 -D wpa_cli       /usr/bin/wpa_cli
install -m755 -D wpa_passphrase /usr/bin/wpa_passphrase

install -d -m755 /usr/share/dbus-1/system-services
install -m644 dbus/fi.w1.wpa_supplicant1.service \
              /usr/share/dbus-1/system-services/fi.w1.wpa_supplicant1.service

install -d -m755 /usr/share/dbus-1/system.d
install -m644 dbus/dbus-wpa_supplicant.conf \
              /usr/share/dbus-1/system.d/wpa_supplicant.conf

install -d -m755 /etc/wpa_supplicant

cd /sources

# --- gates --------------------------------------------------------------------

echo "=== gate: the binaries ==="
test -e /usr/sbin/wpa_supplicant
test -e /usr/bin/wpa_cli
test -e /usr/bin/wpa_passphrase
ls -la /usr/sbin/wpa_supplicant /usr/bin/wpa_cli /usr/bin/wpa_passphrase

echo "=== gate: version paths run under Fil-C ==="
/usr/bin/wpa_cli -v | head -1
/usr/sbin/wpa_supplicant -v | head -1

echo "=== gate: nl80211 driver is compiled in (help lists it) ==="
/usr/sbin/wpa_supplicant -h | grep -E "nl80211" || { echo "FATAL: nl80211 driver missing"; exit 1; }

echo "=== gate: the dbus activation file (Exec -> the installed daemon) ==="
test -e /usr/share/dbus-1/system-services/fi.w1.wpa_supplicant1.service
cat /usr/share/dbus-1/system-services/fi.w1.wpa_supplicant1.service
grep -q "Exec=/usr/sbin/wpa_supplicant -u" /usr/share/dbus-1/system-services/fi.w1.wpa_supplicant1.service

echo "=== gate: the dbus policy ==="
test -e /usr/share/dbus-1/system.d/wpa_supplicant.conf
grep -q "fi.w1.wpa_supplicant1" /usr/share/dbus-1/system.d/wpa_supplicant.conf

echo "=== gate: linked against libnl-3 + dbus (the Fil-C built libs) ==="
ldd /usr/sbin/wpa_supplicant | grep -E "libnl-3|libdbus-1|libssl|libcrypto" || ldd /usr/sbin/wpa_supplicant
if ldd /usr/sbin/wpa_supplicant 2>&1 | grep -q 'not found'; then
    echo "FATAL: wpa_supplicant has unresolved shared libraries:" >&2
    ldd /usr/sbin/wpa_supplicant 2>&1 | grep 'not found' >&2
    exit 1
fi
echo "ldd: clean"

rm -rf pizlonated-wpa_supplicant
hash -r

echo "postlc3 wpa_supplicant: ALL GATES GREEN"
