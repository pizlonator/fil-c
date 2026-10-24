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
test "x$lfsbuildstate" = "xpostlc6"

SRCDIR=$PWD

echo "postlc7-part" > $LFS/sources/lfsbuildstate

FILCOWNER=`stat -c %U $FILCSRC`
id -u $FILCOWNER
su $FILCOWNER ./build_postlc7_sub1_packaging.sh

./build_unmount.sh
./build_mount.sh

cp -v $FILCSRC/projects/*/pizlonated-*.tar.gz $LFS/sources
cp -v build_postlc7_sub2_chroot.sh $LFS/sources
cp -v build_postlc7_chroot_project_*.sh $LFS/sources
cp -v build_postlc7_chroot_setup_scripts_for_gnome.sh $LFS/sources
# Sprint 7a: the GL/EGL stack is rebuilt in this stage from the folded
# Fil-C fixes (see build_postlc7_sub2_chroot.sh); copy its chroot scripts.
cp -v build_postlc4_chroot_project_mesa.sh $LFS/sources
cp -v build_postlc6_chroot_project_libepoxy.sh $LFS/sources
cp -v build_postlc4_chroot_project_gtk4.sh $LFS/sources
# Plain tarball needed by postlc7: the shadow PAM rebuild compares against
# it (the build itself uses pizlonated-shadow.tar.gz, which is already in
# /sources from the earlier stage that built shadow without PAM).
cp -v shadow-4.16.0.tar.xz $LFS/sources

# Sprint 5: session wiring (dbus/elogind bootscripts, GNOME session file,
# autostart desktops, launchers).  build_postlc7_sub2_chroot.sh invokes
# build_postlc7_chroot_setup_scripts_for_gnome.sh at the END of the chroot
# phase; the setup script reads these from /sources/etc.
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

./build_chroot_late.sh /sources/build_postlc7_sub2_chroot.sh

# Exactly ONE build_chroot_late call in this stage: the session wiring
# (build_postlc7_chroot_setup_scripts_for_gnome.sh) is installed by sub2
# itself, at the end of the chroot phase, so the postlc7 snapshot below
# already carries it.

echo "postlc7" > $LFS/sources/lfsbuildstate

./build_unmount.sh

cd $LFS
tar -czpf $SRCDIR/lfs-postlc7.tar.gz --exclude='var/coredumps/*' .

echo Post-libc part 7 OK
