#!/bin/bash

set -e
set -x

test $EUID -eq 0
id -u lfs

export LFS=/mnt/lfs

test -d $LFS
test -d $LFS/yolo/kernel-include

export FILCSRC=..
test -d $FILCSRC
test -d $FILCSRC/libpas
test -d $FILCSRC/llvm
test -d $FILCSRC/clang
test -d $FILCSRC/filc
test -d $FILCSRC/projects/yolo-glibc-2.40
test -d $FILCSRC/projects/user-glibc-2.40

SRCDIR=$PWD

FILCOWNER=`stat -c %U $FILCSRC`
id -u $FILCOWNER
su $FILCOWNER ./build_lc_sub1_filc.sh

test -d $FILCSRC/pizfix/lib
test -d $FILCSRC/build/bin
test -e $FILCSRC/pizfix/lib/libpizlo.so
test -e $FILCSRC/build/bin/clang-20
test -e $FILCSRC/projects/user-glibc-2.40/pizlonated-user-glibc.tar.gz

cp -v $FILCSRC/projects/user-glibc-2.40/pizlonated-user-glibc.tar.gz $LFS/sources

# FIXME: It would be cool if I built libpizlo.so in the chroot.
#
# Why it doesn't work: libpas currently strongly depends on clang, or maybe at least some newer
# version of gcc. Maybe that means that libpas builds with the version of GCC that I'm using for LFS?
# Who knows.
cp -v $FILCSRC/pizfix/lib/libpizlo.so $LFS/usr/lib/
cp -v $FILCSRC/pizfix/lib/filc_crt.o $LFS/usr/lib/
cp -v $FILCSRC/pizfix/lib/filc_mincrt.o $LFS/usr/lib/
test -d $LFS/usr/include
cp -v $FILCSRC/pizfix/stdfil-include/*.h $LFS/usr/include/

cp -v $FILCSRC/build/bin/clang-20 $LFS/usr/bin/clang-20
strip $LFS/usr/bin/clang-20
patchelf --set-rpath /yolo/lib $LFS/usr/bin/clang-20
patchelf --set-interpreter /yolo/lib/ld-linux-x86-64.so.2 $LFS/usr/bin/clang-20

rm -rf $LFS/usr/lib/clang
cp -rv $FILCSRC/build/lib/clang $LFS/usr/lib

./build_unmount.sh
./build_copy_stuff.sh
./build_mount.sh

if test -e $LFS/yolo/bin/bash
then
    ./build_chroot.sh /sources/build_lc_sub3_user_chroot.sh
else
    ./build_chroot_late.sh /bin/bash /sources/build_lc_sub3_user_chroot.sh
fi

cp -v $FILCSRC/pizfix/lib/libc++.so $LFS/usr/lib
cp -v $FILCSRC/pizfix/lib/libc++.so.1.0 $LFS/usr/lib
cp -v $FILCSRC/pizfix/lib/libc++abi.so.1.0 $LFS/usr/lib
cp -v $FILCSRC/pizfix/lib/libc++.a $LFS/usr/lib
cp -v $FILCSRC/pizfix/lib/libc++abi.a $LFS/usr/lib

rm -rf $LFS/usr/include/c++
cp -rv $FILCSRC/build/include/c++ $LFS/usr/include
mkdir -p $LFS/usr/include/x86_64-unknown-linux-gnu
rm -rf $LFS/usr/include/x86_64-unknown-linux-gnu/c++
cp -rv $FILCSRC/build/include/x86_64-unknown-linux-gnu/c++ $LFS/usr/include/x86_64-unknown-linux-gnu
