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
test "x$lfsbuildstate" = "xpostlc7"

SRCDIR=$PWD

echo "postlc8-part" > $LFS/sources/lfsbuildstate

FILCOWNER=`stat -c %U $FILCSRC`
id -u $FILCOWNER
su $FILCOWNER ./build_postlc8_sub1_packaging.sh

./build_unmount.sh
./build_mount.sh

cp -v $FILCSRC/projects/*/pizlonated-*.tar.gz $LFS/sources
cp -v build_postlc8_sub2_chroot.sh $LFS/sources
cp -v build_postlc8_chroot_project_*.sh $LFS/sources

./build_chroot_late.sh /sources/build_postlc8_sub2_chroot.sh

echo "postlc8" > $LFS/sources/lfsbuildstate

./build_unmount.sh

cd $LFS
tar -czpf $SRCDIR/lfs-postlc8.tar.gz --exclude='var/coredumps/*' .

echo Post-libc part 8 OK
