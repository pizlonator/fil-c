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
test "x$lfsbuildstate" = "xpostlc9"

SRCDIR=$PWD

echo "postlc10-part" > $LFS/sources/lfsbuildstate

FILCOWNER=`stat -c %U $FILCSRC`
id -u $FILCOWNER
su $FILCOWNER ./build_postlc10_sub1_packaging.sh

./build_unmount.sh
./build_mount.sh

./build_preseed_pizlix.sh

cp -v $FILCSRC/projects/*/pizlonated-*.tar.gz $LFS/sources
cp -v build_postlc10_sub2_chroot.sh $LFS/sources
cp -v build_postlc10_chroot_project_*.sh $LFS/sources

./build_chroot_late.sh /sources/build_postlc10_sub2_chroot.sh

echo "postlc10" > $LFS/sources/lfsbuildstate

./build_unmount.sh

cd $LFS
tar -czpf $SRCDIR/lfs-postlc10.tar.gz --exclude='var/coredumps/*' --exclude='hostbuild/*' .

echo Post-libc part 10 OK
