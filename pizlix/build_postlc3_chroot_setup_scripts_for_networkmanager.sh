#!/bin/bash

set -e
set -x

ulimit -c unlimited

# Wire NetworkManager into the boot.
#
# The decision this script implements: NM takes over ALL devices -- eth0 AND
# the wifi radios.  That
# means the LFS `network` bootscript (dhcpcd on eth0 via
# /etc/sysconfig/ifconfig.eth0) must NOT run alongside NM: two DHCP
# clients on eth0 fight over the lease, the address and
# /etc/resolv.conf.  Consequences, deliberately chosen:
#
# 1. The blfs-bootscripts `install-networkmanager` target wires
#    /etc/rc.d/init.d/networkmanager as S33networkmanager in rc2..rc5 and
#    K28 in rc0/1/6 (S33 lands after S20dbus + S25elogind, which NM needs
#    for its session tracking + the dbus-activated wpa_supplicant).
# 2. The S20network symlinks are REMOVED from rc3.d/rc5.d.  The `network`
#    script itself and /etc/sysconfig/ifconfig.eth0 stay on disk -- rescue
#    use only (link the symlink back by hand; NM must be stopped first).
# 3. The hostname is NOT lost by dropping S20network: the hostname is set
#    by the `localnet` bootscript (rcS.d S08) from /etc/hostname
#    ("pizlix"), which is independent of the network script (verified:
#    init.d/localnet:31 `[ -r /etc/hostname ] && HOSTNAME=...` :41
#    `hostname ${HOSTNAME}`).  The GNOME session wants a hostname on the
#    bus -- it keeps being set in rcS, before any runlevel script.
# 4. NM's DHCP is the internal nettools client (built with
#    -Dconfig_dhcp_default=internal) and DNS is written directly to
#    /etc/resolv.conf (rc-manager=auto, the default) -- no dhcpcd, no
#    dhclient, no systemd-resolved involvement.
#
# wpa_supplicant gets NO bootscript on purpose: NM D-Bus-activates it
# (fi.w1.wpa_supplicant1) on demand and respawns it if it dies
# (nm-supplicant-manager.c).
#
# The bootscript tarball is the tracked blfs-bootscripts-20240416.tar.xz
# staged into /sources by build_postlc3.sh.

cd /sources
rm -rf blfs-bootscripts-20240416
tar -xf blfs-bootscripts-20240416.tar.xz
cd blfs-bootscripts-20240416

# The blfs target: /etc/rc.d/init.d/networkmanager + the rcN.d symlinks.
make install-networkmanager

cd /sources
rm -rf blfs-bootscripts-20240416

# NM replaces the LFS network script in the managed runlevels (see the
# header).  rc2/rc4 never had S20network links here.
rm -vf /etc/rc.d/rc3.d/S20network /etc/rc.d/rc5.d/S20network

# NM configuration.  Everything else (keyfile plugin, wifi backend
# wpa_supplicant, rc-manager auto) is NM 1.48's default.
install -d -m755 /etc/NetworkManager/conf.d
cat > /etc/NetworkManager/NetworkManager.conf << "EOF"
# Pizlix: NetworkManager manages ALL devices (eth0 + wifi).  The DHCP
# client is NM's internal (vendored nettools) one -- no external DHCP
# client runs (the LFS `network` bootscript / dhcpcd are disabled in the
# managed runlevels; see build_postlc3_chroot_setup_scripts_for_
# networkmanager.sh).  DNS is written directly to /etc/resolv.conf
# (rc-manager=auto, the default).
[main]
dhcp=internal
EOF

# --- gates ---------------------------------------------------------------------

echo "=== gate: the bootscript installed ==="
test -e /etc/rc.d/init.d/networkmanager
grep -q "NetworkManager" /etc/rc.d/init.d/networkmanager

echo "=== gate: the rc symlinks (S33 after dbus S20 + elogind S25, K28 on shutdown) ==="
for n in 2 3 4 5; do
    test -e /etc/rc.d/rc${n}.d/S33networkmanager
done
for n in 0 1 6; do
    test -e /etc/rc.d/rc${n}.d/K28networkmanager
done
ls -la /etc/rc.d/rc3.d/ /etc/rc.d/rc5.d/

echo "=== gate: ordering -- S33networkmanager sorts after S20dbus + S25elogind ==="
test "$(basename $(readlink -f /etc/rc.d/rc5.d/S33networkmanager))" = networkmanager
ls /etc/rc.d/rc5.d/ | sort | grep -nE "S20dbus|S25elogind|S33networkmanager"

echo "=== gate: the LFS network script is out of the managed runlevels (script kept for rescue) ==="
test ! -e /etc/rc.d/rc3.d/S20network
test ! -e /etc/rc.d/rc5.d/S20network
test -e /etc/rc.d/init.d/network
test -e /etc/sysconfig/ifconfig.eth0

echo "=== gate: the hostname does not depend on the network script ==="
test -e /etc/hostname
cat /etc/hostname
grep -q "hostname" /etc/rc.d/init.d/localnet
test -e /etc/rc.d/rcS.d/S08localnet

echo "=== gate: NM config ==="
test -e /etc/NetworkManager/NetworkManager.conf
grep -q "dhcp=internal" /etc/NetworkManager/NetworkManager.conf
ls -la /etc/NetworkManager/

echo "=== gate: NM + wpa_supplicant dbus files in place ==="
test -e /usr/share/dbus-1/system.d/org.freedesktop.NetworkManager.conf
test -e /usr/share/dbus-1/system.d/wpa_supplicant.conf
test -e /usr/share/dbus-1/system-services/fi.w1.wpa_supplicant1.service

hash -r

echo "postlc3 networkmanager boot wiring: ALL GATES GREEN"
