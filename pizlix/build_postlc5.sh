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

test -e /mnt/lfs/sources/lfsbuildstate
lfsbuildstate=`cat /mnt/lfs/sources/lfsbuildstate`
test "x$lfsbuildstate" = "xpostlc4"

SRCDIR=$PWD

echo "postlc5-part" > /mnt/lfs/sources/lfsbuildstate

FILCOWNER=`stat -c %U $FILCSRC`
id -u $FILCOWNER
su $FILCOWNER ./build_postlc5_sub1_packaging.sh

./build_unmount.sh
./build_mount.sh

cp -v $FILCSRC/projects/*/pizlonated-*.tar.gz $LFS/sources
cp -v sqlite-autoconf-3460100.tar.gz $LFS/sources
cp -v build_postlc5_sub2_chroot.sh $LFS/sources
cp -v gsettings-desktop-schemas-46.1.tar.xz $LFS/sources
cp -v libsecret-0.21.4.tar.xz $LFS/sources
cp -v libseccomp-2.5.5.tar.gz $LFS/sources

./build_chroot_late.sh /sources/build_postlc5_sub2_chroot.sh

echo "postlc5" > /mnt/lfs/sources/lfsbuildstate

./build_unmount.sh

cd $LFS
tar -czpf $SRCDIR/lfs-postlc5.tar.gz --exclude='var/coredumps/*' .

echo Post-libc part 5 OK

