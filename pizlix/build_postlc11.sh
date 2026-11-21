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

# The WiFi stage's Sprint A: the kernel gets CONFIG_MAC80211_HWSIM=m (the
# mac80211 hardware simulator -- virtual radios for QEMU testing of the
# cfg80211/mac80211 stack; =m so real boots do not auto-create radios) and
# the target gets the iwlwifi firmware subset + the wens-signed regulatory
# db in /lib/firmware.  No new projeny packages in Sprint A; the two
# firmware tarballs come from sub1's download cache.
./build_preseed_pizlix.sh

# The chroot scripts: the kernel rebuild (drop-in of part2's kernel block)
# and the manifest-driven firmware install.  The kernel script runs first
# (sub2 orders them that way): the firmware selection reads the kernel's
# own modules.builtin.modinfo manifest.
cp -v build_postlc11_sub2_chroot.sh $LFS/sources
cp -v build_postlc11_chroot_project_*.sh $LFS/sources

# Sprint B of the WiFi-on-Pizlix port: the WiFi userspace stack.  The four
# projeny-packaged tarballs (libnl -> libndp -> wpa_supplicant ->
# NetworkManager) plus the boot-wiring setup script (the blfs
# networkmanager bootscript + the rcN.d wiring) and the tracked
# blfs-bootscripts tarball the setup script installs the bootscript from.
cp -v $FILCSRC/projects/libnl/pizlonated-libnl.tar.gz $LFS/sources
cp -v $FILCSRC/projects/libndp/pizlonated-libndp.tar.gz $LFS/sources
cp -v $FILCSRC/projects/wpa_supplicant/pizlonated-wpa_supplicant.tar.gz $LFS/sources
cp -v $FILCSRC/projects/networkmanager/pizlonated-networkmanager.tar.gz $LFS/sources
cp -v build_postlc11_chroot_setup_scripts_for_networkmanager.sh $LFS/sources
cp -v blfs-bootscripts-20240416.tar.xz $LFS/sources

# The two firmware tarballs (downloaded into pizlix/ by sub1_packaging --
# too big to commit to git) staged into /sources for the chroot firmware
# script.
cp -v $FILCSRC/pizlix/linux-firmware-20241017.tar.xz $LFS/sources
cp -v $FILCSRC/pizlix/wireless-regdb-master-2024-10-07.tar.gz $LFS/sources

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