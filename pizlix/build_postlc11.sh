#!/bin/bash

set -e
set -x

ulimit -c unlimited

test $EUID -eq 0
id -u lfs

export LFS=/mnt/lfs

test -d $LFS

export FILCSRC=..
test -d $FILCSRC/projects

test -e $LFS/sources/lfsbuildstate
lfsbuildstate=`cat $LFS/sources/lfsbuildstate`
test "x$lfsbuildstate" = "xpostlc10"

SRCDIR=$PWD

echo "postlc11-part" > $LFS/sources/lfsbuildstate

FILCOWNER=`stat -c %U $FILCSRC`
id -u $FILCOWNER
su $FILCOWNER ./build_postlc11_sub1_packaging.sh

./build_unmount.sh
./build_mount.sh

# The kernel gets CONFIG_MAC80211_HWSIM=m (the
# mac80211 hardware simulator -- virtual radios for QEMU testing of the
# cfg80211/mac80211 stack; =m so real boots do not auto-create radios) and
# the target gets the iwlwifi firmware subset + the wens-signed regulatory
# db in /lib/firmware.  No new projeny packages here; the two
# firmware tarballs come from sub1's download cache.
./build_preseed_pizlix.sh

# The chroot scripts: the kernel rebuild (drop-in of part2's kernel block)
# and the manifest-driven firmware install.  The kernel script runs first
# (sub2 orders them that way): the firmware selection reads the kernel's
# own modules.builtin.modinfo manifest.
cp -v build_postlc11_sub2_chroot.sh $LFS/sources
cp -v build_postlc11_chroot_project_*.sh $LFS/sources

# The WiFi userspace stack tarballs
# (libnl / libndp / wpa_supplicant / NetworkManager) are now POSTLC7
# packages -- they moved there because g-c-c (postlc7) links libnm +
# libnma-gtk4 and gnome-shell (postlc8) needs libnm + the NMA4 typelib;
# build_postlc7.sh copies them via its projects/*/pizlonated-*.tar.gz
# wildcard.  This stage keeps only the boot wiring (setup) script and the
# blfs-bootscripts tarball it installs the bootscript from.
cp -v build_postlc11_chroot_setup_scripts_for_networkmanager.sh $LFS/sources
cp -v blfs-bootscripts-20240416.tar.xz $LFS/sources

# The firmware inputs (staged into /sources for the chroot firmware
# script): the 379MB linux-firmware tarball (downloaded into pizlix/ by
# sub1_packaging -- too big to commit to git) and, since the 2026-11-22
# projeny conversion, the wireless-regdb package tarball (built by sub1
# from projects/wireless-regdb.projeny -- the git-tracked
# pizlix/wireless-regdb-master-2024-10-07.tar.gz is gone).
cp -v $FILCSRC/pizlix/linux-firmware-20241017.tar.xz $LFS/sources
cp -v $FILCSRC/projects/wireless-regdb/pizlonated-wireless-regdb.tar.gz $LFS/sources

# The tracked kernel config, copied next to the kernel script that consumes
# it: the full chain already gets it from build_copy_stuff.sh, but the
# recovery chain (recover postlc10 -> build_postlc11_continuation) starts
# from a snapshot whose /sources/linux-config-6.10.5 predates the HWSIM
# line, so the driver ships the current one.
cp -v $FILCSRC/pizlix/linux-config-6.10.5 $LFS/sources

./build_chroot_late.sh /sources/build_postlc11_sub2_chroot.sh

echo "postlc11" > $LFS/sources/lfsbuildstate

./build_unmount.sh

cd $LFS
tar -czpf $SRCDIR/lfs-postlc11.tar.gz --exclude='var/coredumps/*' --exclude='hostbuild/*' .

echo Post-libc part 11 OK