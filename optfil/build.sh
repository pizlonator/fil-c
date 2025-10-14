#!/bin/sh
#
# Copyright (c) 2025 Epic Games, Inc. All Rights Reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY EPIC GAMES, INC. ``AS IS AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL EPIC GAMES, INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

set -e
set -x

ulimit -c unlimited

test $EUID -eq 0

FILCSRC=$PWD/..

test -d $FILCSRC/filc
test -d $FILCSRC/projects

FILCOWNER=`stat -c %U $FILCSRC`
id -u $FILCOWNER
su $FILCOWNER ./build_filc.sh

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

cd /opt

rm -rf fil
mkdir -v fil
cd fil

cp -r $FILCSRC/optfil/kernel-include include

mkdir -v build
cd build

tar -xf $FILCSRC/projects/yolo-glibc-2.40/pizlonated-yolo-glibc.tar.gz
cd pizlonated-yolo-glibc
mkdir -v build
cd build
../configure --prefix=/opt/fil --disable-mathvec --disable-nscd libc_cv_slibdir=/opt/fil/lib
make -j `nproc`
touch /opt/fil/etc/ld.so.conf
sed '/test-installation/s@$(PERL)@echo not running@' -i ../Makefile
make -j `nproc` install
cd ../..
test -d pizlonated-yolo-glibc
cd ..
test -d build
cd ..
test -d fil

rm -rf fil-yolo
mv fil fil-yolo
mkdir -pv fil/lib

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
cp -v fil-yolo/lib/$OLDLDNAME fil/lib/$LDNAME
cp -v fil-yolo/lib/$OLDLIBCIMPLNAME fil/lib/$LIBCIMPLNAME
cp -v fil-yolo/lib/$OLDLIBCNONSHAREDNAME fil/lib/$LIBCNONSHAREDNAME
cp -v fil-yolo/lib/$OLDLIBMIMPLNAME fil/lib/$LIBMIMPLNAME
cp -v fil-yolo/lib/*.o fil/lib/
patchelf --replace-needed $OLDLDNAME $LDNAME fil/lib/$LIBCIMPLNAME
patchelf --set-soname $LIBCIMPLNAME fil/lib/$LIBCIMPLNAME
patchelf --set-soname $LDNAME fil/lib/$LDNAME
patchelf --replace-needed $OLDLDNAME $LDNAME fil/lib/$LIBMIMPLNAME
patchelf --replace-needed $OLDLIBCIMPLNAME $LIBCIMPLNAME fil/lib/$LIBMIMPLNAME
patchelf --set-soname $LIBMIMPLNAME fil/lib/$LIBMIMPLNAME
echo "OUTPUT_FORMAT(elf64-x86-64)" > fil/lib/$LIBCNAME
echo "GROUP ( /opt/fil/lib/$LIBCIMPLNAME /opt/fil/lib/$LIBCNONSHAREDNAME  AS_NEEDED ( /opt/fil/lib/$LDNAME ) )" >> fil/lib/$LIBCNAME
echo "OUTPUT_FORMAT(elf64-x86-64)" > fil/lib/$LIBMNAME
echo "GROUP ( /opt/fil/lib/$LIBMIMPLNAME )" >> fil/lib/$LIBMNAME
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
rm -rf fil-yolo

# FIXME: It would be cool if I built libpizlo.so here.
cp -v $FILCSRC/pizfix/$FILCLIB/libpizlo.so fil/lib/
cp -v $FILCSRC/pizfix/lib/filc_crt.o fil/lib/
cp -v $FILCSRC/pizfix/lib/filc_mincrt.o fil/lib/
cp -r $FILCSRC/optfil/kernel-include fil/include
cp -v $FILCSRC/pizfix/stdfil-include/*.h fil/include/

mkdir -v fil/bin
cp -v $FILCSRC/build/bin/clang-20 fil/bin/filcc-clang-20
strip fil/bin/filcc-clang-20
patchelf --remove-rpath fil/bin/filcc-clang-20

cp -rv $FILCSRC/build/lib/clang fil/lib

ln -s filcc-clang-20 fil/bin/filcc
ln -s filcc-clang-20 fil/bin/fil++
ln -s filcc-clang-20 fil/bin/filcpp

cd fil
mkdir -v build
cd build
tar -xf $FILCSRC/projects/user-glibc-2.40/pizlonated-user-glibc.tar.gz
cd pizlonated-user-glibc
mkdir -v build
cd build
echo "rootsbindir=/opt/fil/sbin" > configparms
CC="/opt/fil/bin/filcc -nostdlibinc -Wno-ignored-attributes -Wno-pointer-sign" CXX="/opt/fil/bin/fil++ -nostdlibinc -Wno-ignored-attributes -Wno-pointer-sign" ../configure --prefix=/opt/fil \
    --disable-werror \
    --enable-kernel=4.19 \
    --disable-nscd \
    --disable-mathvec \
    libc_cv_slibdir=/opt/fil/lib
make -j `nproc`
touch /opt/fil/etc/ld.so.conf
sed '/test-installation/s@$(PERL)@echo not running@' -i ../Makefile
make -j `nproc` install
# FIXME: Do we need to do the localdef/zic stuff that Pizlix does?
cd ../..
test -d pizlonated-user-glibc
cd ..
test -d build
rm -rf build

cp -v $FILCSRC/pizfix/lib/libc++.so lib
cp -v $FILCSRC/pizfix/lib/libc++.so.1.0 lib
cp -v $FILCSRC/pizfix/lib/libc++abi.so.1.0 lib
cp -v $FILCSRC/pizfix/lib/libc++.a lib
cp -v $FILCSRC/pizfix/lib/libc++abi.a lib
ln -s libc++.so.1.0 lib/libc++.so.1
ln -s libc++abi.so.1.0 lib/libc++abi.so.1
ln -s libc++abi.so.1 lib/libc++abi.so
cp -rv $FILCSRC/build/include/c++ include
mkdir -p include/x86_64-unknown-linux-gnu
cp -rv $FILCSRC/build/include/x86_64-unknown-linux-gnu/c++ include/x86_64-unknown-linux-gnu

