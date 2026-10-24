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

# The session glue is NOT re-installed here: postlc7 owns the session
# wiring (build_postlc7_chroot_setup_scripts_for_gnome.sh runs at the end
# of postlc7's sub2), so the postlc7 snapshot this stage builds on already
# carries it.  This stage has exactly ONE build_chroot_late call.

./build_chroot_late.sh /sources/build_postlc8_sub2_chroot.sh

echo "postlc8" > $LFS/sources/lfsbuildstate

./build_unmount.sh

cd $LFS
tar -czpf $SRCDIR/lfs-postlc8.tar.gz --exclude='var/coredumps/*' .

echo Post-libc part 8 OK
