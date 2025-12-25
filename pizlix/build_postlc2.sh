#!/bin/bash

set -e
set -x

# This step takes ~14 mins on exodus

ulimit -c unlimited

test $EUID -eq 0
id -u lfs

export LFS=/mnt/lfs

test -d $LFS

export FILCSRC=..
test -d $FILCSRC/projects

test -e $LFS/sources/lfsbuildstate
lfsbuildstate=`cat $LFS/sources/lfsbuildstate`
test "x$lfsbuildstate" = "xpostlc"

SRCDIR=$PWD

echo "postlc2-part" > $LFS/sources/lfsbuildstate

FILCOWNER=`stat -c %U $FILCSRC`
id -u $FILCOWNER
su $FILCOWNER ./build_postlc2_sub1_packaging.sh

./build_unmount.sh
./build_mount.sh

cp -v $FILCSRC/projects/*/pizlonated-*.tar.gz $LFS/sources
cp -v which-2.21.tar.gz $LFS/sources
cp -v brotli-1.1.0.tar.gz $LFS/sources
cp -v libunistring-1.2.tar.xz $LFS/sources
cp -v libpsl-0.21.5.tar.gz $LFS/sources
cp -v make-ca-1.16.1.tar.gz $LFS/sources
cp -v nghttp2-1.62.1.tar.xz $LFS/sources
cp -v pcre2-10.44.tar.bz2 $LFS/sources
cp -v wget-1.24.5.tar.gz $LFS/sources
cp -v blfs-bootscripts-20240416.tar.xz $LFS/sources
cp -v build_postlc2_sub2_chroot.sh $LFS/sources
cp -v build_postlc2_chroot_project_dhcpcd.sh $LFS/sources
cp -v build_postlc2_chroot_project_libidn2.sh $LFS/sources

./build_chroot_late.sh /sources/build_postlc2_sub2_chroot.sh

echo "postlc2" > $LFS/sources/lfsbuildstate

./build_unmount.sh

cd $LFS
tar -czpf $SRCDIR/lfs-postlc2.tar.gz --exclude='var/coredumps/*' .

echo Post-libc part 2 OK

