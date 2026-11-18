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

# Sprint W1+W2 of the Epiphany-on-Pizlix port: the postlc10 stage is the
# WebKitGTK build (webkitgtk-2.44.3, the webkitgtk-6.0 API -- the GTK4
# flavor Epiphany 46 links) plus the projeny packages Epiphany itself will
# need in W3 (epiphany, libportal, desktop-file-utils; they are packaged
# and shipped here so the W3 epiphany stage can just build them).
#
# All four tarballs are projeny/packaging products under projects/*/ and
# are picked up by the same glob postlc5-postlc8 use.  pizlonated-
# webkitgtk.tar.gz comes from package-source.sh (the checked-in
# projects/webkitgtk-2.44.3 tree); the other three from `filc/projeny
# package` (their .projeny files are new in this sprint).
#
# The chroot script removes its build dir and extracted tree after a
# successful install, so the stage snapshot stays small (the 55MB tarball
# stays in /sources).
cp -v $FILCSRC/projects/*/pizlonated-*.tar.gz $LFS/sources
cp -v build_postlc10_sub2_chroot.sh $LFS/sources
cp -v build_postlc10_chroot_project_*.sh $LFS/sources

./build_chroot_late.sh /sources/build_postlc10_sub2_chroot.sh

echo "postlc10" > $LFS/sources/lfsbuildstate

./build_unmount.sh

cd $LFS
tar -czpf $SRCDIR/lfs-postlc10.tar.gz --exclude='var/coredumps/*' --exclude='hostbuild/*' .

echo Post-libc part 10 OK
