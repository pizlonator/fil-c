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

test -e $LFS/sources/lfsbuildstate
lfsbuildstate=`cat $LFS/sources/lfsbuildstate`
test "x$lfsbuildstate" = "xprelc"

SRCDIR=$PWD

echo "lc-part" > $LFS/sources/lfsbuildstate

FILCOWNER=`stat -c %U $FILCSRC`
id -u $FILCOWNER
su $FILCOWNER ./build_lc_sub1_filc.sh

if test -n "$FILCLIB"
then
    echo "*******************************************************************************"
    echo
    echo "FILCLIB is set!"
    echo "Using $FILCSRC/pizfix/$FILCLIB/libpizlo.so"
    echo
    echo "*******************************************************************************"
else
    FILCLIB=lib
fi

test -d $FILCSRC/pizfix/lib
test -d $FILCSRC/build/bin
test -e $FILCSRC/pizfix/$FILCLIB/libpizlo.so
test -e $FILCSRC/build/bin/clang-20
test -e $FILCSRC/projects/yolo-glibc-2.40/pizlonated-yolo-glibc.tar.gz
test -e $FILCSRC/projects/user-glibc-2.40/pizlonated-user-glibc.tar.gz

./build_unmount.sh
./build_yoloify.sh
./build_lc_make_usr.sh
./build_copy_stuff.sh
./build_mount.sh

cp -v $FILCSRC/projects/yolo-glibc-2.40/pizlonated-yolo-glibc.tar.gz $LFS/sources
cp -v $FILCSRC/projects/user-glibc-2.40/pizlonated-user-glibc.tar.gz $LFS/sources

./build_chroot.sh /sources/build_lc_sub2_yolo_chroot.sh

mv $LFS/usr $LFS/yolo-glibc-prefix
./build_lc_make_usr.sh

OLDLDNAME=ld-linux-x86-64.so.2
OLDLIBCIMPLNAME=libc.so.6
OLDLIBCNONSHAREDNAME=libc_nonshared.a
OLDLIBMIMPLNAME=libm.so.6
LDNAME=ld-yolo-x86_64.so
LIBNAMEBASE=libyolo
LIBCNAMEBASE=${LIBNAMEBASE}c
LIBCNAME=${LIBCNAMEBASE}.so
LIBCIMPLNAME=${LIBCNAMEBASE}impl.so
LIBCNONSHAREDNAME=${LIBCNAMEBASE}_nonshared.a
LIBMIMPLNAME=${LIBNAMEBASE}mimpl.so
LIBMNAME=${LIBNAMEBASE}m.so
cp -v $LFS/yolo-glibc-prefix/lib/$OLDLDNAME $LFS/usr/lib/$LDNAME
cp -v $LFS/yolo-glibc-prefix/lib/$OLDLIBCIMPLNAME $LFS/usr/lib/$LIBCIMPLNAME
cp -v $LFS/yolo-glibc-prefix/lib/$OLDLIBCNONSHAREDNAME $LFS/usr/lib/$LIBCNONSHAREDNAME
cp -v $LFS/yolo-glibc-prefix/lib/$OLDLIBMIMPLNAME $LFS/usr/lib/$LIBMIMPLNAME
cp -v $LFS/yolo-glibc-prefix/lib/*.o $LFS/usr/lib/
patchelf --replace-needed $OLDLDNAME $LDNAME $LFS/usr/lib/$LIBCIMPLNAME
patchelf --set-soname $LIBCIMPLNAME $LFS/usr/lib/$LIBCIMPLNAME
patchelf --set-soname $LDNAME $LFS/usr/lib/$LDNAME
patchelf --replace-needed $OLDLDNAME $LDNAME $LFS/usr/lib/$LIBMIMPLNAME
patchelf --replace-needed $OLDLIBCIMPLNAME $LIBCIMPLNAME $LFS/usr/lib/$LIBMIMPLNAME
patchelf --set-soname $LIBMIMPLNAME $LFS/usr/lib/$LIBMIMPLNAME
echo "OUTPUT_FORMAT(elf64-x86-64)" > $LFS/usr/lib/$LIBCNAME
echo "GROUP ( /usr/lib/$LIBCIMPLNAME /usr/lib/$LIBCNONSHAREDNAME  AS_NEEDED ( /usr/lib/$LDNAME ) )" >> $LFS/usr/lib/$LIBCNAME
echo "OUTPUT_FORMAT(elf64-x86-64)" > $LFS/usr/lib/$LIBMNAME
echo "GROUP ( /usr/lib/$LIBMIMPLNAME )" >> $LFS/usr/lib/$LIBMNAME
unset OLDLDNAME
unset OLDLIBCIMPLNAME
unset OLDLIBCNONSHAREDNAME
unset OLDLIBMIMPLNAME
unset LDNAME
unset LIBNAMEBASE
unset LIBCNAMEBASE
unset LIBCNAME
unset LIBCIMPLNAME
unset LIBCNONSHAREDNAME
unset LIBMIMPLNAME
unset LIBMNAME
rm -rf $LFS/yolo-glibc-prefix

# FIXME: It would be cool if I built libpizlo.so in the chroot.
#
# Why it doesn't work: libpas currently strongly depends on clang, or maybe at least some newer
# version of gcc. Maybe that means that libpas builds with the version of GCC that I'm using for LFS?
# Who knows.
cp -v $FILCSRC/pizfix/$FILCLIB/libpizlo.so $LFS/usr/lib/
cp -v $FILCSRC/pizfix/lib/filc_crt.o $LFS/usr/lib/
cp -v $FILCSRC/pizfix/lib/filc_mincrt.o $LFS/usr/lib/
cp -v $FILCSRC/pizfix/stdfil-include/*.h $LFS/yolo/kernel-include

cp -v $FILCSRC/build/bin/clang-20 $LFS/usr/bin/clang-20
strip $LFS/usr/bin/clang-20
patchelf --set-rpath /yolo/lib $LFS/usr/bin/clang-20
patchelf --set-interpreter /yolo/lib/ld-linux-x86-64.so.2 $LFS/usr/bin/clang-20

cp -rv $FILCSRC/build/lib/clang $LFS/usr/lib

ln -s clang-20 $LFS/usr/bin/clang
ln -s clang-20 $LFS/usr/bin/clang++
ln -s clang-20 $LFS/usr/bin/cpp
ln -s clang-20 $LFS/usr/bin/gcc
ln -s clang-20 $LFS/usr/bin/g++
ln -s clang-20 $LFS/usr/bin/cc
ln -s clang-20 $LFS/usr/bin/c++

./build_chroot.sh /sources/build_lc_sub3_user_chroot.sh

# FIXME: It would be so much better if I build libc++/libc++abi in the chroot.
#
# Reason why it isn't right now: because the libc++/libc++abi builds are cmake-based and currently I
# have them set up to build with ninja, and the bootstrapping environment has neither.
cp -v $FILCSRC/pizfix/lib/libc++.so $LFS/usr/lib
cp -v $FILCSRC/pizfix/lib/libc++.so.1.0 $LFS/usr/lib
cp -v $FILCSRC/pizfix/lib/libc++abi.so.1.0 $LFS/usr/lib
cp -v $FILCSRC/pizfix/lib/libc++.a $LFS/usr/lib
cp -v $FILCSRC/pizfix/lib/libc++abi.a $LFS/usr/lib
ln -s libc++.so.1.0 $LFS/usr/lib/libc++.so.1
ln -s libc++abi.so.1.0 $LFS/usr/lib/libc++abi.so.1
ln -s libc++abi.so.1 $LFS/usr/lib/libc++abi.so
cp -rv $FILCSRC/build/include/c++ $LFS/usr/include
mkdir -p $LFS/usr/include/x86_64-unknown-linux-gnu
cp -rv $FILCSRC/build/include/x86_64-unknown-linux-gnu/c++ $LFS/usr/include/x86_64-unknown-linux-gnu

echo "lc" > $LFS/sources/lfsbuildstate

./build_unmount.sh
cd $LFS
tar -czpf $SRCDIR/lfs-lc.tar.gz --exclude='var/coredumps/*' .

echo libc OK
