#!/bin/bash

set -e
set -x

ulimit -c unlimited

# Sprint B of the WiFi-on-Pizlix port: NetworkManager 1.48.4 -- the network
# daemon the GNOME shell talks to (libnm + NM-1.0 typelib for the network
# indicator/quick settings; the wifi backend is wpa_supplicant over dbus).
#
# Meson flags, every optional dependency pinned to what the chroot has
# (meson_options.txt read; the recon's flag set verified against it):
#
#   session_tracking/suspend_resume = elogind   chroot has libelogind.pc
#     (elogind 255.27; the elogind branch asserts libelogind >= 229) --
#     NM's session watch (network control for active users via polkit) and
#     sleep/suspend inhibition ride elogind, same as gnome-shell.
#   session_tracking_consolekit=false           no consolekit
#   polkit=true                                 polkit 126 in chroot; NM's
#     shipped org.freedesktop.NetworkManager.policy lands in polkit's
#     actions dir (the default policy is allow_active for the user-facing
#     actions, so pizlo controls NM through the shell's agent)
#   selinux=false, libaudit=no, systemd_journal=false   none in chroot
#   systemdunitdir=no                           no systemd units (sysvinit)
#   modem_manager=false, iwd=false, ppp=false, ovs=false,
#     teamdctl=false, bluez5_dun=false          the optional networking
#     backends absent from Pizlix (teamdctl would also drag in jansson)
#   nmtui=false                                 libnewt absent
#   nm_cloud_setup=false, ebpf=false            cloud/bpf features off
#   ifupdown=false                              no /etc/network/interfaces
#   qt=false                                    no Qt
#   crypto=gnutls                               gnutls 3.8.7 (NM's cert/key
#     handling for 802.1x; nss is the upstream default but absent here)
#   wifi=true, wext=false                       the point of the sprint;
#     wext (old wireless-extensions ioctls) is dead, nl80211 only
#   concheck=true                               libcurl 8.23 present; NM's
#     connectivity check (the GNOME portal-detection helper)
#   dhcp=internal                               NM's vendored nettools DHCP --
#     no dhclient/dhcpcd plugin, no external DHCP client
#   config_dns_rc_manager_default=auto          NM writes /etc/resolv.conf
#     directly (no systemd-resolved, no resolvconf)
#   introspection=true, vapi=false              NM-1.0.gir/typelib for gjs
#     (the shell imports gi://NM); no vala in chroot
#   tests=no                                    the unit tests need more
#     infra than the chroot has; nothing consumes them
#   docs=false, more_asserts=0                  no gtk-doc; production-like
#     assert level (buildtype stays debugoptimized per house style)
#   ld_gc=false                                 no -ffunction-sections
#     --gc-sections linking (the house pattern from the libnma recon:
#     avoid gc-sections quirks under Fil-C)
#
# perl + xsltproc are unconditional find_program()s in meson.build (man
# page generation) -- both verified present in the chroot.  jansson is
# probed required:false (absent OK: NM's JSON config parser compiles out).
#
# udev rules (84-nm-drivers.rules, 85-nm-unmanaged.rules) install to the
# udevdir from udev.pc (systemd 256's udev) -- left at the default, they
# are correct runtime hints (e.g. marking WWAN devices unmanaged).

cd /sources
rm -rf pizlonated-networkmanager
tar -xf pizlonated-networkmanager.tar.gz
cd pizlonated-networkmanager
mkdir -v build
cd build
meson setup ..                   \
      --prefix=/usr              \
      --buildtype=debugoptimized \
      -Dsystemdsystemunitdir=no  \
      -Dsystemd_journal=false    \
      -Dsession_tracking=elogind \
      -Dsession_tracking_consolekit=false \
      -Dsuspend_resume=elogind   \
      -Dpolkit=true              \
      -Dselinux=false            \
      -Dlibaudit=no              \
      -Dmodem_manager=false      \
      -Diwd=false                \
      -Dppp=false                \
      -Dovs=false                \
      -Dteamdctl=false           \
      -Dbluez5_dun=false         \
      -Dnmtui=false              \
      -Dnm_cloud_setup=false     \
      -Debpf=false               \
      -Difupdown=false           \
      -Dqt=false                 \
      -Dcrypto=gnutls            \
      -Dwifi=true                \
      -Dwext=false               \
      -Dconcheck=true            \
      -Dconfig_dhcp_default=internal \
      -Dconfig_dns_rc_manager_default=auto \
      -Dintrospection=true       \
      -Dvapi=false               \
      -Dtests=no                 \
      -Ddocs=false               \
      -Dfirewalld_zone=false     \
      -Dmore_asserts=0           \
      -Dld_gc=false
ninja
ninja install
cd ../..

# --- gates ---------------------------------------------------------------------

echo "=== gate: the daemon + clients ==="
test -e /usr/sbin/NetworkManager
test -e /usr/bin/nmcli
test -e /usr/bin/nm-online
ls -la /usr/sbin/NetworkManager /usr/bin/nmcli /usr/bin/nm-online
/usr/bin/nmcli --version

echo "=== gate: the client library + introspection (gjs needs the typelib) ==="
test -e /usr/lib/libnm.so
test -e /usr/lib/pkgconfig/libnm.pc
pkg-config --modversion libnm
test "x`pkg-config --modversion libnm`" = "x1.48.4"
test -e /usr/share/gir-1.0/NM-1.0.gir
test -e /usr/lib/girepository-1.0/NM-1.0.typelib
ls -la /usr/share/gir-1.0/NM-1.0.gir /usr/lib/girepository-1.0/NM-1.0.typelib

echo "=== gate: the dbus wiring (system.d policy + the helper activation services) ==="
# NB: NM 1.48.4 does NOT install a dbus system-services file for ITSELF
# (the daemon is started by the S30networkmanager bootscript, not
# dbus-activated) -- the org.freedesktop.nm_dispatcher / nm_priv_helper
# service files are what lands in system-services, plus the main
# org.freedesktop.NetworkManager.conf policy in system.d.
test -e /usr/share/dbus-1/system.d/org.freedesktop.NetworkManager.conf
test -e /usr/share/dbus-1/system-services/org.freedesktop.nm_dispatcher.service
test -e /usr/share/dbus-1/system-services/org.freedesktop.nm_priv_helper.service
cat /usr/share/dbus-1/system-services/org.freedesktop.nm_dispatcher.service

echo "=== gate: the polkit policy (pizlo's allow_active authority) ==="
test -e /usr/share/polkit-1/actions/org.freedesktop.NetworkManager.policy
grep -c "allow_active" /usr/share/polkit-1/actions/org.freedesktop.NetworkManager.policy

echo "=== gate: the config + dispatcher dirs ==="
test -d /etc/NetworkManager
ls -la /etc/NetworkManager/

echo "=== gate: gjs can load the NM typelib ==="
gjs -c "const NM = imports.gi.NM; print('NM', NM.MAJOR_VERSION, NM.MINOR_VERSION);"

echo "=== gate: ldd of the binaries (no 'not found') ==="
for f in /usr/sbin/NetworkManager /usr/bin/nmcli /usr/bin/nm-online /usr/lib/libnm.so; do
    if ldd $f 2>&1 | grep -q 'not found'; then
        echo "FATAL: $f has unresolved shared libraries:" >&2
        ldd $f 2>&1 | grep 'not found' >&2
        exit 1
    fi
done
echo "ldd: clean"

rm -rf pizlonated-networkmanager
hash -r

echo "postlc11 networkmanager: ALL GATES GREEN"
