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
test "x$lfsbuildstate" = "xpostlc2"

SRCDIR=$PWD

echo "postlc3-part" > $LFS/sources/lfsbuildstate

FILCOWNER=`stat -c %U $FILCSRC`
id -u $FILCOWNER
su $FILCOWNER ./build_postlc3_sub1_packaging.sh

./build_unmount.sh
./build_mount.sh

cp -v $FILCSRC/projects/*/pizlonated-*.tar.gz $LFS/sources
cp -v wayland-protocols-1.45.tar.xz $LFS/sources
cp -v xkeyboard-config-2.42.tar.xz $LFS/sources
cp -v pixman-0.43.4.tar.gz $LFS/sources
cp -v packaging-24.1.tar.gz $LFS/sources
cp -v daemon-0.6.4.tar.gz $LFS/sources
cp -v build_postlc3_sub2_chroot.sh $LFS/sources
cp -v build_postlc3_chroot_project_glib.sh $LFS/sources
cp -v build_postlc3_chroot_subproject_gobject_introspection.sh $LFS/sources
cp -v build_postlc3_chroot_project_cairo.sh $LFS/sources
cp -v build_postlc3_chroot_project_weston.sh $LFS/sources
cp -v build_postlc3_chroot_project_seatd.sh $LFS/sources
cp -v build_postlc3_chroot_project_libdrm.sh $LFS/sources
cp -v build_postlc3_chroot_setup_scripts_for_weston.sh $LFS/sources
cp -v etc/weston.ini $LFS/sources/etc
cp -v etc/profile $LFS/sources/etc
cp -v etc/profile-xdg-runtime-dir.sh $LFS/sources/etc
cp -v etc/seatd $LFS/sources/etc
cp -v graphite2-1.3.14.tgz $LFS/sources
cp -v build_postlc3_chroot_project_freetype.sh $LFS/sources
cp -v build_postlc3_chroot_project_graphite.sh $LFS/sources
cp -v build_postlc3_chroot_project_fontconfig.sh $LFS/sources
cp -v build_postlc3_chroot_project_harfbuzz.sh $LFS/sources

./build_chroot_late.sh /sources/build_postlc3_sub2_chroot.sh

mkdir -pv $LFS/usr/share/fonts
cp -rv dejavu $LFS/usr/share/fonts/

echo "postlc3" > $LFS/sources/lfsbuildstate

./build_unmount.sh

cd $LFS
tar -czpf $SRCDIR/lfs-postlc3.tar.gz --exclude='var/coredumps/*' .

echo Post-libc part 3 OK

