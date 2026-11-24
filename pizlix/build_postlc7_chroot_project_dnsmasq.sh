#!/bin/bash

set -e
set -x

ulimit -c unlimited

# dnsmasq 2.90 -- the lightweight
# DNS/DHCP server NetworkManager spawns for SHARED-mode connections (Wi-Fi
# hotspot/tethering, connection sharing).  A live `nmcli device wifi
# hotspot` check got through the 802.11 part
# (supplicant state inactive -> completed, "Started Wi-Fi Hotspot") but
# died at IP config with:
#
#   ip[shared4]: could not start dnsmasq: Could not find "dnsmasq" binary
#   device (wlan1): state change: ip-config -> failed (reason
#     'ip-config-unavailable')
#
# NM 1.48's shared-mode IP setup drives the dnsmasq BINARY by command line
# (it is not an in-process server like the internal nettools DHCP client
# that serves eth0's lease -- the client works, the shared-mode server is
# this external program).  Without it, hotspots can never hand out leases.
#
# Classic single-Makefile build (no configure): `make PREFIX=/usr` +
# `make install PREFIX=/usr` puts the binary in /usr/sbin/dnsmasq, which is
# in NM's binary search path.  The example config lands in /etc/dnsmasq.conf
# (BLFS pattern; NM always runs dnsmasq with --conf-file=/dev/null, so the
# file is documentation only).  No dbus service needed for NM's use.
#
# Fil-C: compiled clean with zero patches (plain sockets + poll code).

cd /sources
rm -rf pizlonated-dnsmasq
tar -xf pizlonated-dnsmasq.tar.gz
cd pizlonated-dnsmasq
make -j`nproc` PREFIX=/usr
make install PREFIX=/usr
install -m 644 dnsmasq.conf.example /etc/dnsmasq.conf
cd ../..

# --- gates ---------------------------------------------------------------------

echo "=== gate: the binary + config ==="
test -e /usr/sbin/dnsmasq
test -e /etc/dnsmasq.conf
ls -la /usr/sbin/dnsmasq /etc/dnsmasq.conf
/usr/sbin/dnsmasq --version | head -1

echo "=== gate: ldd (no 'not found') ==="
if ldd /usr/sbin/dnsmasq 2>&1 | grep -q 'not found'; then
    echo "FATAL: dnsmasq has unresolved shared libraries:" >&2
    ldd /usr/sbin/dnsmasq 2>&1 | grep 'not found' >&2
    exit 1
fi
echo "ldd: clean"

rm -rf pizlonated-dnsmasq
hash -r

echo "postlc7 dnsmasq: ALL GATES GREEN"
