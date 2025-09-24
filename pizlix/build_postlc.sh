#!/bin/bash

set -e
set -x

ulimit -c unlimited

test $EUID -eq 0
id -u lfs

export LFS=/mnt/lfs

test -d $LFS

export FILCSRC=..
test -d $FILCSRC
test -d $FILCSRC/libpas
test -d $FILCSRC/llvm
test -d $FILCSRC/clang
test -d $FILCSRC/filc
test -d $FILCSRC/projects

test -e /mnt/lfs/sources/lfsbuildstate
lfsbuildstate=`cat /mnt/lfs/sources/lfsbuildstate`
test "x$lfsbuildstate" = "xlc"

SRCDIR=$PWD

echo "postlc-part" > /mnt/lfs/sources/lfsbuildstate

FILCOWNER=`stat -c %U $FILCSRC`
id -u $FILCOWNER
su $FILCOWNER ./build_postlc_sub1_filc.sh

./build_unmount.sh
./build_copy_stuff.sh

cp -v $FILCSRC/projects/*/pizlonated-*.tar.gz $LFS/sources

test ! -d $LFS/sources/emacs-lisp
cp -rv $FILCSRC/projects/emacs-lisp $LFS/sources

./build_mount.sh
./build_chroot.sh /sources/build_postlc_sub2_chroot_part1.sh
echo "postlc" > /mnt/lfs/sources/lfsbuildstate

./build_unmount.sh
cd $LFS
tar -czpf $SRCDIR/lfs-postlc.tar.gz --exclude='var/coredumps/*' .

echo Post-libc OK
