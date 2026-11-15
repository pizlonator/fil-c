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
# `projeny package` into projects/librsvg/), and so are the four
# Fil-C-patched crates (glib-sys/gobject-sys/glib/string_cache: the GType
# is a pointer for Fil-C, and string_cache keeps capability-carrying
# dynamic-atom Entry pointers).  The root directory inside each crate
# tarball is <crate>/ so unpacking into /sources yields the paths the
# librsvg workspace's [patch.crates-io] section points at (../glib-sys
# etc. from /sources/pizlonated-librsvg).
cp -v $FILCSRC/rust-build/pizlonated-filrustc.tar.gz $LFS/sources
cp -v $FILCSRC/rust-build/rust-yolo-1.90.0.tar.gz $LFS/sources
cp -v $FILCSRC/rust-build/vendor-librsvg.tar.gz $LFS/sources
cp -v $FILCSRC/projects/librsvg/pizlonated-librsvg.tar.gz $LFS/sources
cp -v $FILCSRC/projects/glib-sys/glib-sys.tar.gz $LFS/sources
cp -v $FILCSRC/projects/gobject-sys/gobject-sys.tar.gz $LFS/sources
cp -v $FILCSRC/projects/glib/glib.tar.gz $LFS/sources
cp -v $FILCSRC/projects/string_cache/string_cache.tar.gz $LFS/sources
cp -v build_postlc9_sub2_chroot.sh $LFS/sources
cp -v build_postlc9_chroot_project_rust.sh $LFS/sources
cp -v build_postlc9_chroot_project_rust_dispatch.sh $LFS/sources
cp -v build_postlc9_chroot_project_librsvg.sh $LFS/sources

./build_chroot_late.sh /sources/build_postlc9_sub2_chroot.sh

echo "postlc9" > $LFS/sources/lfsbuildstate

./build_unmount.sh

cd $LFS
tar -czpf $SRCDIR/lfs-postlc9.tar.gz --exclude='var/coredumps/*' --exclude='hostbuild/*' .

echo Post-libc part 9 OK