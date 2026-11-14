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
test "x$lfsbuildstate" = "xpostlc8"

SRCDIR=$PWD

echo "postlc9-part" > $LFS/sources/lfsbuildstate

FILCOWNER=`stat -c %U $FILCSRC`
id -u $FILCOWNER
su $FILCOWNER ./build_postlc9_sub1_packaging.sh

./build_unmount.sh
./build_mount.sh

# The toolchain tarballs are not projeny products: sub1_packaging builds
# them into $FILCSRC/rust-build/ (the untracked rust build-output dir) and
# they are copied explicitly here (the shadow-tarball pattern: special
# files, explicit cp lines).  Same for the librsvg vendored dependencies;
# pizlonated-librsvg.tar.gz IS a projeny product (sub1_packaging runs
# `projeny package` into projects/librsvg/).
cp -v $FILCSRC/rust-build/pizlonated-filrustc.tar.gz $LFS/sources
cp -v $FILCSRC/rust-build/rust-yolo-1.90.0.tar.gz $LFS/sources
cp -v $FILCSRC/rust-build/vendor-librsvg.tar.gz $LFS/sources
cp -v $FILCSRC/projects/librsvg/pizlonated-librsvg.tar.gz $LFS/sources
# Sprint S3 support: the glib GType capability-repair rebuild (its tarball is
# a package-source.sh product, like the other pre-projeny packages).
cp -v $FILCSRC/projects/glib-2.80.4/pizlonated-glib.tar.gz $LFS/sources
cp -v build_postlc9_sub2_chroot.sh $LFS/sources
cp -v build_postlc9_chroot_project_rust.sh $LFS/sources
cp -v build_postlc9_chroot_project_rust_dispatch.sh $LFS/sources
cp -v build_postlc9_chroot_project_glib_gtyperepair.sh $LFS/sources
cp -v build_postlc9_chroot_project_librsvg.sh $LFS/sources

./build_chroot_late.sh /sources/build_postlc9_sub2_chroot.sh

echo "postlc9" > $LFS/sources/lfsbuildstate

./build_unmount.sh

cd $LFS
tar -czpf $SRCDIR/lfs-postlc9.tar.gz --exclude='var/coredumps/*' --exclude='hostbuild/*' .

echo Post-libc part 9 OK