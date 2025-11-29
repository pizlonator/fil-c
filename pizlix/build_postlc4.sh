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
test "x$lfsbuildstate" = "xpostlc3"

SRCDIR=$PWD

echo "postlc4-part" > /mnt/lfs/sources/lfsbuildstate

FILCOWNER=`stat -c %U $FILCSRC`
id -u $FILCOWNER
su $FILCOWNER ./build_postlc4_sub1_packaging.sh

./build_unmount.sh
./build_mount.sh

cp -v $FILCSRC/projects/*/pizlonated-*.tar.gz $LFS/sources
cp -v build_postlc4_sub2_chroot.sh $LFS/sources
cp -v fribidi-1.0.15.tar.xz $LFS/sources
cp -v shared-mime-info-2.4.tar.gz $LFS/sources
cp -v pycairo-1.26.1.tar.gz $LFS/sources
cp -v build_postlc4_chroot_project_graphene.sh $LFS/sources
cp -v build_postlc4_chroot_project_pygobject.sh $LFS/sources
cp -v iso-codes_4.16.0.orig.tar.xz $LFS/sources
cp -v Mako-1.3.5.tar.gz $LFS/sources
cp -v build_postlc4_chroot_project_mesa.sh $LFS/sources
cp -v libepoxy-1.5.10.tar.xz $LFS/sources

./build_chroot_late.sh /sources/build_postlc4_sub2_chroot.sh

echo "postlc4" > /mnt/lfs/sources/lfsbuildstate

./build_unmount.sh

cd $LFS
tar -czpf $SRCDIR/lfs-postlc4.tar.gz --exclude='var/coredumps/*' .

echo Post-libc part 4 OK

