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

# Sprint C of the WiFi-on-Pizlix port: the WiFi USERSPACE stack moved to
# the postlc7 (GNOME desktop) stage -- gnome-control-center (postlc7)
# links libnm + libnma-gtk4 and gnome-shell (postlc8) links libnm and
# statically imports gi://NMA4, so the stack (libnl -> libndp ->
# wpa_supplicant -> NetworkManager -> libnma) must be installed BEFORE
# those stages run; leaving it here (postlc11) would kill a from-scratch
# build in g-c-c's meson setup.  postlc11 keeps only: the kernel rebuild,
# the firmware subset, and the boot wiring below (setup, not a build
# dependency).

# Sprint B: the boot wiring -- the blfs networkmanager bootscript
# (S33networkmanager in rc2-5 after S20dbus + S25elogind, K28 on
# shutdown/halt), the S20network (dhcpcd) symlinks dropped from the
# managed runlevels (NM manages eth0 too; the script stays on disk for
# rescue), and /etc/NetworkManager/NetworkManager.conf.  The hostname is
# unaffected: localnet (rcS.d S08) sets it from /etc/hostname.
# (The NetworkManager PACKAGE installs in postlc7 since Sprint C; this
# boot wiring stays here because the full chain reaches postlc11 after
# postlc7/postlc8, and wiring the bootscripts is a setup step, not a
# build dependency.)
./build_postlc11_chroot_setup_scripts_for_networkmanager.sh
hash -r