#!/bin/bash

set -e
set -x

# This step takes ~95 mins (estimate: X11 + GNOME desktop + shell + kernel/firmware).
# This stage merges the old postlc6 (everything except LibreOffice) + postlc7
# + postlc8 + postlc11 stages: X11 libraries and the LibreOffice dependencies,
# the GNOME desktop and GNOME Shell, then the kernel HWSIM rebuild, the WiFi
# firmware and the NetworkManager boot wiring.  The GL/EGL stack is NOT
# rebuilt here: postlc2 builds mesa/libepoxy/gtk4 ONCE from the fixed projeny
# package / fixed headers (the old layout's cross-stage GL rebuilds are gone).

ulimit -c unlimited

test $EUID -eq 0
id -u lfs

export LFS=/mnt/lfs

test -d $LFS

export FILCSRC=..
test -d $FILCSRC/projects

test -e $LFS/sources/lfsbuildstate
lfsbuildstate=`cat $LFS/sources/lfsbuildstate`
test "x$lfsbuildstate" = "xpostlc2"

SRCDIR=$PWD

echo "postlc3-part" > $LFS/sources/lfsbuildstate

FILCOWNER=`stat -c %U $FILCSRC`
id -u $FILCOWNER
su $FILCOWNER ./build_postlc3_sub1_packaging.sh

./build_unmount.sh
./build_mount.sh

# Staging: everything this stage's chroot script consumes (the recovery
# chain never re-runs build_copy_stuff.sh, so each stage stages its own
# inputs).  The lines below are the union of the merged stages' staging
# (old postlc6 minus LibreOffice + old postlc7 + postlc8 + postlc11), in
# build order; identical lines deduped (LibreOffice moved to postlc5, and
# the old postlc7 GL re-run script copies are gone with the re-runs).

cp -v $FILCSRC/projects/*/pizlonated-*.tar.gz $LFS/sources

# X11 libraries + LibreOffice dependencies (the old postlc6 stage minus
# LibreOffice, which is postlc5's payload now).
cp -v build_postlc3_sub2_chroot.sh $LFS/sources
cp -v boost_1_86_0.tar.bz2 $LFS/sources
cp -v boost-filc.patch $LFS/sources
cp -v zip30.tar.gz $LFS/sources
cp -v util-macros-*.tar.xz $LFS/sources
cp -v xorgproto-*.tar.xz $LFS/sources
cp -v xtrans-*.tar.xz $LFS/sources
cp -v xcb-proto-*.tar.xz $LFS/sources
cp -v libxcb-*.tar.xz $LFS/sources
cp -v libX*.tar.xz $LFS/sources
cp -v libICE-*.tar.xz $LFS/sources
cp -v libSM-*.tar.xz $LFS/sources
cp -v libepoxy-1.5.10.tar.xz $LFS/sources
cp -v gl_stub.c $LFS/sources

# The GNOME desktop + Shell + WiFi (the old postlc7 + postlc8 + postlc11
# stages).  One wildcard stages every build_postlc3_chroot_project_*.sh
# (the X11/boost/glstub/libxslt/zip/unzip scripts of the old postlc6 stage
# plus the desktop/shell/WiFi scripts); the two setup scripts are staged
# explicitly, like the old drivers did.
cp -v build_postlc3_chroot_project_*.sh $LFS/sources
cp -v build_postlc3_chroot_setup_scripts_for_gnome.sh $LFS/sources

# The plain shadow tarball rides along (the PAM rebuild extracts the
# pizlonated-shadow tarball that the postlc stage left in /sources); staged
# here so this stage's snapshot is self-contained.
cp -v shadow-4.16.0.tar.xz $LFS/sources

# Session wiring (dbus/elogind bootscripts, GNOME session file, autostart
# desktops, launchers).  build_postlc3_sub2_chroot.sh invokes
# build_postlc3_chroot_setup_scripts_for_gnome.sh at the end of the desktop
# block (before the GNOME Shell block and the WiFi tail, which in the old
# layout were later stages that ran after this wiring anyway); the setup
# script reads these from /sources/etc.
mkdir -pv $LFS/sources/etc/init.d
mkdir -pv $LFS/sources/etc/gnome-session
mkdir -pv $LFS/sources/etc/xdg/autostart
mkdir -pv $LFS/sources/etc/gnome
cp -v etc/init.d/dbus etc/init.d/elogind $LFS/sources/etc/init.d
cp -v etc/gnome-session/pizlix-gnome.session $LFS/sources/etc/gnome-session
cp -v etc/xdg/autostart/org.gnome.Shell.desktop \
      etc/xdg/autostart/pizlix-terminal.desktop $LFS/sources/etc/xdg/autostart
cp -v etc/gnome/pizlix-gnome.sh etc/gnome/pizlix-shell.sh \
      etc/gnome/pizlix-mutter.sh $LFS/sources/etc/gnome

# The WiFi tail of the stage (the old postlc11 stage): the kernel HWSIM
# rebuild, the manifest-driven firmware install, and the NetworkManager boot
# wiring.  The kernel gets CONFIG_MAC80211_HWSIM=m (the mac80211 hardware
# simulator -- virtual radios for QEMU testing of the cfg80211/mac80211
# stack; =m so real boots do not auto-create radios) and the target gets the
# iwlwifi firmware subset + the wens-signed regulatory db in /lib/firmware.
# No new projeny packages here; the two firmware tarballs come from sub1's
# download cache + the projeny wildcard above.
#
# The chroot scripts: the kernel rebuild (drop-in of part2's kernel block)
# and the manifest-driven firmware install ride in the
# build_postlc3_chroot_project_*.sh wildcard above (the kernel script runs
# first -- sub2 orders them that way: the firmware selection reads the
# kernel's own modules.builtin.modinfo manifest).
cp -v build_postlc3_chroot_setup_scripts_for_networkmanager.sh $LFS/sources

# The WiFi boot wiring installs the networkmanager bootscript out of this
# tarball (the blfs-bootscripts copy staged by postlc2's driver is for its
# dhcpcd/sshd services; this stage is self-contained).
cp -v blfs-bootscripts-20240416.tar.xz $LFS/sources

# The firmware inputs (staged into /sources for the chroot firmware
# script): the 379MB linux-firmware tarball (downloaded into pizlix/ by
# sub1_packaging -- too big to commit to git; pizlonated-wireless-regdb
# comes via the projeny wildcard above).
cp -v $FILCSRC/pizlix/linux-firmware-20241017.tar.xz $LFS/sources

# The tracked kernel config, copied next to the kernel script that consumes
# it: the full chain already gets it from build_copy_stuff.sh, but the
# recovery chain (recover postlc2 -> build_postlc3_continuation) starts
# from a snapshot whose /sources/linux-config-6.10.5 predates the HWSIM
# line, so the driver ships the current one.
cp -v $FILCSRC/pizlix/linux-config-6.10.5 $LFS/sources

./build_chroot_late.sh /sources/build_postlc3_sub2_chroot.sh

echo "postlc3" > $LFS/sources/lfsbuildstate

./build_unmount.sh

cd $LFS
tar -czpf $SRCDIR/lfs-postlc3.tar.gz --exclude='var/coredumps/*' .

echo Post-libc part 3 OK
