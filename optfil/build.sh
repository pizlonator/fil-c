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

export FILCSRC=$PWD/..

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
../configure --prefix=/opt/fil --disable-mathvec --disable-nscd libc_cv_slibdir=/opt/fil/lib --with-headers=/opt/fil/include
make -j `nproc`
mkdir -p /opt/fil/etc
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
mkdir -p /opt/fil/etc
touch /opt/fil/etc/ld.so.conf
sed '/test-installation/s@$(PERL)@echo not running@' -i ../Makefile
make -j `nproc` install
# FIXME: Do we need to do the localdef/zic stuff that Pizlix does?
cd ../..
test -d pizlonated-user-glibc
rm -rf pizlonated-user-glibc
cd ..

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

cd build
tar -xf $FILCSRC/pizlix/zlib-1.3.1.tar.gz
cd zlib-1.3.1
./configure --prefix=/opt/fil
make -j `nproc`
make -j `nproc` install
rm -fv /opt/fil/lib/libz.a
cd ..
rm -rf zlib-1.3.1

tar -xf $FILCSRC/pizlix/bzip2-1.0.8.tar.gz
cd bzip2-1.0.8
sed -i 's@\(ln -s -f \)$(PREFIX)/bin/@\1@' Makefile
sed -i "s@(PREFIX)/man@(PREFIX)/share/man@g" Makefile
make -j `nproc` -f Makefile-libbz2_so
make -j `nproc` clean
make -j `nproc`
make -j `nproc` PREFIX=/opt/fil install
cp -av libbz2.so.* /opt/fil/lib
ln -sv libbz2.so.1.0.8 /opt/fil/lib/libbz2.so
cp -v bzip2-shared /opt/fil/bin/bzip2
for i in /opt/fil/bin/{bzcat,bunzip2}; do
    ln -sfv bzip2 $i
done
rm -fv /opt/fil/lib/libbz2.a
cd ..
rm -rf bzip2-1.0.8

tar -xf $FILCSRC/projects/xz-5.6.2/pizlonated-xz.tar.gz
cd pizlonated-xz
./configure --prefix=/opt/fil \
    --disable-static \
    --docdir=/opt/fil/share/doc/xz-5.6.2
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf pizlonated-xz

tar -xf $FILCSRC/pizlix/lz4-1.10.0.tar.gz
cd lz4-1.10.0
make -j `nproc` BUILD_STATIC=no PREFIX=/opt/fil
make -j `nproc` BUILD_STATIC=no PREFIX=/opt/fil install
cd ..
rm -rf lz4-1.10.0

tar -xf $FILCSRC/pizlix/zstd-1.5.6.tar.gz
cd zstd-1.5.6
ZSTD_NO_ASM=1 make -j `nproc` prefix=/opt/fil
ZSTD_NO_ASM=1 make -j `nproc` prefix=/opt/fil install
rm -v /opt/fil/lib/libzstd.a
cd ..
rm -rf zstd-1.5.6

tar -xf $FILCSRC/projects/pkgconf-2.3.0/pizlonated-pkgconf.tar.gz
cd pizlonated-pkgconf
./configure --prefix=/opt/fil \
    --disable-static \
    --docdir=/opt/fil/share/doc/pkgconf-2.3.0
make -j `nproc`
make -j `nproc` install
ln -sv pkgconf /opt/fil/bin/pkg-config
ln -sv pkgconf.1 /opt/fil/share/man/man1/pkg-config.1
cd ..
rm -rf pizlonated-pkgconf

tar -xf $FILCSRC/projects/libxcrypt-4.4.36/pizlonated-libxcrypt.tar.gz
cd pizlonated-libxcrypt
./configure --prefix=/opt/fil \
    --enable-hashes=strong,glibc \
    --enable-obsolete-api=no \
    --disable-static \
    --disable-failure-tokens
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf pizlonated-libxcrypt

tar -xf $FILCSRC/projects/bash-5.2.32/pizlonated-bash.tar.gz
cd pizlonated-bash
./configure --prefix=/opt/fil \
    --without-bash-malloc \
    --with-installed-readline \
    bash_cv_strtold_broken=no \
    --docdir=/opt/fil/share/doc/bash-5.2.32
make -j `nproc`
make -j `nproc` install

tar -xf $FILCSRC/projects/openssl-3.3.1/pizlonated-openssl.tar.gz
cd pizlonated-openssl
./config --prefix=/opt/fil \
    --openssldir=/etc/ssl \
    --libdir=lib \
    shared \
    zlib-dynamic
make -j `nproc`
sed -i '/INSTALL_LIBS/s/libcrypto.a libssl.a//' Makefile
make -j `nproc` MANSUFFIX=ssl install
mv -v /opt/fil/share/doc/openssl /opt/fil/share/doc/openssl-3.3.1
cp -vfr doc/* /opt/fil/share/doc/openssl-3.3.1
cd ..
rm -rf pizlonated-openssl

tar -xf $FILCSRC/projects/libffi-3.4.6/pizlonated-libffi.tar.gz
cd pizlonated-libffi
./configure --prefix=/opt/fil \
    --disable-static \
    --with-gcc-arch=native \
    --disable-exec-static-tramp
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf pizlonated-libffi

tar -xf $FILCSRC/pizlix/coreutils-9.5.tar.xz
cd coreutils-9.5
patch -Np1 -i $FILCSRC/pizlix/coreutils-9.5-i18n-2.patch
autoreconf -fiv
FORCE_UNSAFE_CONFIGURE=1 ./configure \
    --prefix=/opt/fil \
    --enable-no-install-program=kill,uptime
make -j `nproc`
make -j `nproc` install
mv -v /opt/fil/bin/chroot /opt/fil/sbin
mv -v /opt/fil/share/man/man1/chroot.1 /opt/fil/share/man/man8/chroot.8
sed -i 's/"1"/"8"/' /opt/fil/share/man/man8/chroot.8
cd ..
rm -rf coreutils-9.5

tar -xf $FILCSRC/pizlix/mg-3.7.tar.gz 
cd mg-3.7
./configure --prefix=/opt/fil --without-curses
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf mg-3.7

tar -xf $FILCSRC/projects/openssh-9.8p1/pizlonated-openssh.tar.gz
cd pizlonated-openssh
install -v -m700 -d /opt/fil/var/lib/sshd &&
./configure --prefix=/opt/fil \
            --sysconfdir=/opt/fil/etc/ssh \
            --with-privsep-path=/opt/fil/var/lib/sshd \
            --with-default-path=/opt/fil/bin:/usr/bin:/bin \
            --with-superuser-path=/opt/fil/sbin:/opt/fil/bin:/usr/sbin:/usr/bin:/bin \
            --with-pid-dir=/run
make -j `nproc`
make -j `nproc` install
install -v -m755    contrib/ssh-copy-id /opt/fil/bin
install -v -m644    contrib/ssh-copy-id.1 \
                    /opt/fil/share/man/man1
install -v -m755 -d /opt/fil/share/doc/openssh-9.8p1
install -v -m644    INSTALL LICENCE OVERVIEW README* \
                    /opt/fil/share/doc/openssh-9.8p1
cd ..
rm -rf pizlonated-openssh
cd ..
test -d build
rm -rf build

