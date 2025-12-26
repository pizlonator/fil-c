#!/bin/bash

set -e
set -x

ulimit -c unlimited

test $EUID -eq 0
id -u lfs

SRCDIR=$PWD

export LFS=/mnt/lfs

test -d $LFS

export FILCSRC=..
test -d $FILCSRC
test -d $FILCSRC/projects

FILCOWNER=`stat -c %U $FILCSRC`
id -u $FILCOWNER
su $FILCOWNER ./build_prelc_sub0_filc.sh

./build_unmount.sh

(cd $LFS && rm -rf *)

mkdir -v $LFS/sources
chmod -v a+wt $LFS/sources

echo "prelc-part" > $LFS/sources/lfsbuildstate

./build_copy_stuff.sh
cp -v $FILCSRC/projects/*/pizlonated-*.tar.gz $LFS/sources

mkdir -pv $LFS/{etc,var,usr} $LFS/yolo/{bin,lib,sbin}
for i in bin lib sbin; do
    ln -sv ../yolo/$i $LFS/usr/$i
    ln -sv usr/$i $LFS/$i
done
mkdir -pv $LFS/lib64

mkdir -pv $LFS/tools

chown -v lfs $LFS/{yolo{,/*},usr,lib,var,etc,bin,sbin,tools}
chown -v lfs $LFS/lib64

su lfs ./build_prelc_sub1_lfsuser_trampoline.sh

./build_chown_from_lfs_to_root.sh
./build_mount.sh
./build_chroot.sh /sources/build_prelc_sub2_chroot_part1.sh
echo "prelc" > $LFS/sources/lfsbuildstate

./build_unmount.sh
cd $LFS
tar -czpf $SRCDIR/lfs-prelc.tar.gz --exclude='var/coredumps/*' .

echo Pre-libc OK

