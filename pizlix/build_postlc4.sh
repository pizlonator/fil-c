#!/bin/bash

set -e
set -x

# This step takes ~115 mins (estimate: rust + librsvg + webkitgtk + epiphany).
# This stage merges the old postlc9 + postlc10 stages: the Rust toolchains,
# librsvg, WebKitGTK and Epiphany.  (The old postlc10 stage's GnuTLS
# trust-store rebuild is gone: postlc2 builds GnuTLS once with the
# pre-seeded CA bundle as its default trust store.)

ulimit -c unlimited

test $EUID -eq 0
id -u lfs

export LFS=/mnt/lfs

test -d $LFS

export FILCSRC=..
test -d $FILCSRC/projects

test -e $LFS/sources/lfsbuildstate
lfsbuildstate=`cat $LFS/sources/lfsbuildstate`
test "x$lfsbuildstate" = "xpostlc3"

SRCDIR=$PWD

echo "postlc4-part" > $LFS/sources/lfsbuildstate

FILCOWNER=`stat -c %U $FILCSRC`
id -u $FILCOWNER
su $FILCOWNER ./build_postlc4_sub1_packaging.sh

./build_unmount.sh
./build_mount.sh

# Staging: everything this stage's chroot script consumes (the recovery
# chain never re-runs build_copy_stuff.sh, so each stage stages its own
# inputs).  The lines below are the union of the merged stages' staging
# (old postlc9 + postlc10).

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

# The wildcard carries this stage's projeny/package-source products (and
# copies the rust/librsvg project scripts by their
# build_postlc4_chroot_project_*.sh names).
cp -v $FILCSRC/projects/*/pizlonated-*.tar.gz $LFS/sources
cp -v build_postlc4_sub2_chroot.sh $LFS/sources
cp -v build_postlc4_chroot_project_*.sh $LFS/sources

./build_chroot_late.sh /sources/build_postlc4_sub2_chroot.sh

echo "postlc4" > $LFS/sources/lfsbuildstate

./build_unmount.sh

cd $LFS
tar -czpf $SRCDIR/lfs-postlc4.tar.gz --exclude='var/coredumps/*' --exclude='hostbuild/*' .

echo Post-libc part 4 OK
