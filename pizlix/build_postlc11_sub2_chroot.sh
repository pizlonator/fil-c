#!/bin/bash

set -e
set -x

ulimit -c unlimited

cd /sources

# Sprint 11.1 of the WiFi-on-Pizlix port: the kernel rebuild with
# CONFIG_MAC80211_HWSIM=m.  This is a drop-in of part2's kernel block (the
# only kernel change the WiFi stage needs; iwlwifi/cfg80211/mac80211 are
# already =y).  It must run FIRST: the firmware script's manifest-driven
# selection reads /lib/modules/6.10.5/modules.builtin.modinfo, which this
# rebuild refreshes.
./build_postlc11_chroot_project_kernel.sh
hash -r

# Sprint 11.2 of the WiFi-on-Pizlix port: the WiFi firmware subset (the
# manifest-driven iwlwifi ucode/pnvm selection) + the wens-signed
# wireless-regdb, all installed FLAT into /lib/firmware.
./build_postlc11_chroot_project_firmware.sh
hash -r

# Sprint B of the WiFi-on-Pizlix port: the WiFi userspace stack, in
# dependency order:
#   libnl          -- the netlink lib wpa_supplicant's nl80211 driver links
#                     (NetworkManager itself does NOT use libnl)
#   libndp         -- NetworkManager's unconditional IPv6 NDP dependency
#   wpa_supplicant -- the WiFi auth daemon NM D-Bus-activates; needs libnl
#                     (CONFIG_LIBNL32=y) and installs the
#                     fi.w1.wpa_supplicant1 dbus activation service
#   NetworkManager -- needs libndp (hard dep), gnutls/polkit/elogind (in
#                     the chroot), and installs libnm + the NM-1.0
#                     gir/typelib; the daemon D-Bus-activates wpa_supplicant
./build_postlc11_chroot_project_libnl.sh
hash -r
./build_postlc11_chroot_project_libndp.sh
hash -r
./build_postlc11_chroot_project_wpa_supplicant.sh
hash -r
./build_postlc11_chroot_project_networkmanager.sh
hash -r

# Sprint B: the boot wiring -- the blfs networkmanager bootscript
# (S33networkmanager in rc2-5 after S20dbus + S25elogind, K28 on
# shutdown/halt), the S20network (dhcpcd) symlinks dropped from the
# managed runlevels (NM manages eth0 too; the script stays on disk for
# rescue), and /etc/NetworkManager/NetworkManager.conf.  The hostname is
# unaffected: localnet (rcS.d S08) sets it from /etc/hostname.
./build_postlc11_chroot_setup_scripts_for_networkmanager.sh
hash -r