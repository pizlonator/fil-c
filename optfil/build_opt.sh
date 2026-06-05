#!/bin/bash
#
# Copyright (c) 2025 Epic Games, Inc. All Rights Reserved.
# Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
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
# THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
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

test `id -u` -eq 0

export FILCSRC=$PWD/..

test -d $FILCSRC/filc
test -d $FILCSRC/projects
test -d $FILCSRC/optfil

FILCOWNER=`stat -c %U $FILCSRC`
id -u $FILCOWNER
su $FILCOWNER ./build_filc_packages.sh

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

cd /opt/fil
find . -mindepth 1 -maxdepth 1 -exec rm -rf {} \;

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

mkdir -v yolo
mv -v bin etc include lib libexec sbin share var yolo
mkdir -v lib

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
cp -v yolo/lib/$OLDLDNAME lib/$LDNAME
cp -v yolo/lib/$OLDLIBCIMPLNAME lib/$LIBCIMPLNAME
cp -v yolo/lib/$OLDLIBCNONSHAREDNAME lib/$LIBCNONSHAREDNAME
cp -v yolo/lib/$OLDLIBMIMPLNAME lib/$LIBMIMPLNAME
cp -v yolo/lib/*.o lib/
patchelf --replace-needed $OLDLDNAME $LDNAME lib/$LIBCIMPLNAME
patchelf --set-soname $LIBCIMPLNAME lib/$LIBCIMPLNAME
patchelf --set-soname $LDNAME lib/$LDNAME
patchelf --replace-needed $OLDLDNAME $LDNAME lib/$LIBMIMPLNAME
patchelf --replace-needed $OLDLIBCIMPLNAME $LIBCIMPLNAME lib/$LIBMIMPLNAME
patchelf --set-soname $LIBMIMPLNAME lib/$LIBMIMPLNAME
echo "OUTPUT_FORMAT(elf64-x86-64)" > lib/$LIBCNAME
echo "GROUP ( /opt/fil/lib/$LIBCIMPLNAME /opt/fil/lib/$LIBCNONSHAREDNAME  AS_NEEDED ( /opt/fil/lib/$LDNAME ) )" >> lib/$LIBCNAME
echo "OUTPUT_FORMAT(elf64-x86-64)" > lib/$LIBMNAME
echo "GROUP ( /opt/fil/lib/$LIBMIMPLNAME )" >> lib/$LIBMNAME
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
rm -rf yolo

# FIXME: It would be cool if I built libpizlo.so here.
cp -v $FILCSRC/pizfix/$FILCLIB/libpizlo.so lib/
cp -v $FILCSRC/pizfix/lib/filc_crt.o lib/
cp -v $FILCSRC/pizfix/lib/filc_mincrt.o lib/
cp -v $FILCSRC/pizfix/lib/crtbegin.o lib/
cp -v $FILCSRC/pizfix/lib/crtend.o lib/
cp -v $FILCSRC/pizfix/lib/libyolort.a lib/
cp -v $FILCSRC/pizfix/lib/libyolounwind.a lib/
cp -r $FILCSRC/optfil/kernel-include include
cp -v $FILCSRC/pizfix/stdfil-include/*.h include/

mkdir -v bin
cp -v $FILCSRC/build/bin/clang-20 bin/filcc-clang-20
strip bin/filcc-clang-20
patchelf --remove-rpath bin/filcc-clang-20

# This hack only works so long as the host system's glibc is *older* than the glibc that Fil-C uses.
patchelf --set-interpreter /opt/fil/lib/ld-yolo-x86_64.so bin/filcc-clang-20
patchelf --replace-needed ld-linux-x86-64.so.2 ld-yolo-x86_64.so bin/filcc-clang-20
patchelf --replace-needed libc.so.6 libyolocimpl.so bin/filcc-clang-20
patchelf --replace-needed libm.so.6 libyolomimpl.so bin/filcc-clang-20

cp -rv $FILCSRC/build/lib/clang lib

ln -s filcc-clang-20 bin/filcc
ln -s filcc-clang-20 bin/fil++
ln -s filcc-clang-20 bin/filcpp

test -d build
test ../fil
rm -rf build
mkdir -v build
cd build
tar -xf $FILCSRC/projects/user-glibc-2.40/pizlonated-user-glibc.tar.gz
cd pizlonated-user-glibc
mkdir -v build
cd build
echo "rootsbindir=/opt/fil/sbin" > configparms
CC="/opt/fil/bin/filcc -nostdlibinc -Wno-ignored-attributes -Wno-pointer-sign -Wno-unused-command-line-argument -Wno-macro-redefined" CXX="/opt/fil/bin/fil++ -nostdlibinc -Wno-ignored-attributes -Wno-pointer-sign" ../configure --prefix=/opt/fil \
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

export PATH=/opt/fil/bin:$PATH
hash -r

tar -xf $FILCSRC/pizlix/zlib-1.3.1.tar.gz
cd zlib-1.3.1
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil
make -j `nproc`
make -j `nproc` install
rm -fv /opt/fil/lib/libz.a
cd ..
rm -rf zlib-1.3.1
hash -r

tar -xf $FILCSRC/projects/binutils-2.43.1/pizlonated-binutils.tar.gz
cd pizlonated-binutils
mkdir -v build
cd build
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ../configure --prefix=/opt/fil \
    --sysconfdir=/opt/fil/etc \
    --disable-gold \
    --enable-ld=default \
    --enable-plugins \
    --enable-shared \
    --disable-werror \
    --enable-64-bit-bfd \
    --enable-new-dtags \
    --with-system-zlib \
    --enable-default-hash-style=gnu \
    --disable-gprofng
make -j `nproc` tooldir=/opt/fil
make -j `nproc` tooldir=/opt/fil install
rm -fv /opt/fil/lib/lib{bfd,ctf,ctf-nobfd,gprofng,opcodes,sframe}.a
cd ../..
rm -rf pizlonated-binutils
hash -r

tar -xf $FILCSRC/pizlix/bzip2-1.0.8.tar.gz
cd bzip2-1.0.8
sed -i 's@\(ln -s -f \)$(PREFIX)/bin/@\1@' Makefile
sed -i "s@(PREFIX)/man@(PREFIX)/share/man@g" Makefile
make CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ -j `nproc` -f Makefile-libbz2_so
make CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ -j `nproc` clean
make CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ -j `nproc`
make CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ -j `nproc` PREFIX=/opt/fil install
cp -av libbz2.so.* /opt/fil/lib
ln -sv libbz2.so.1.0.8 /opt/fil/lib/libbz2.so
cp -v bzip2-shared /opt/fil/bin/bzip2
for i in /opt/fil/bin/{bzcat,bunzip2}; do
    ln -sfv bzip2 $i
done
rm -fv /opt/fil/lib/libbz2.a
cd ..
rm -rf bzip2-1.0.8
hash -r

tar -xf $FILCSRC/projects/xz-5.6.2/pizlonated-xz.tar.gz
cd pizlonated-xz
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil \
    --disable-static \
    --docdir=/opt/fil/share/doc/xz-5.6.2
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf pizlonated-xz
hash -r

tar -xf $FILCSRC/pizlix/lz4-1.10.0.tar.gz
cd lz4-1.10.0
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ make -j `nproc` BUILD_STATIC=no PREFIX=/opt/fil
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ make -j `nproc` BUILD_STATIC=no PREFIX=/opt/fil install
cd ..
rm -rf lz4-1.10.0
hash -r

tar -xf $FILCSRC/projects/zstd-1.5.6/pizlonated-zstd.tar.gz
cd pizlonated-zstd
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ZSTD_NO_ASM=1 make -j `nproc` prefix=/opt/fil
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ZSTD_NO_ASM=1 make -j `nproc` prefix=/opt/fil install
rm -v /opt/fil/lib/libzstd.a
cd ..
rm -rf pizlonated-zstd
hash -r

tar -xf $FILCSRC/projects/attr-2.5.2/pizlonated-attr.tar.gz
cd pizlonated-attr
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure \
    --prefix=/opt/fil --disable-static --sysconfdir=/etc --docdir=/opt/fil/share/doc/attr-2.5.2
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf pizlonated-attr
hash -r

tar -xf $FILCSRC/pizlix/acl-2.3.2.tar.xz
cd acl-2.3.2
sed -i s/-Wl,--version-script,/-Wc,--version-script=/g Makefile.in
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure \
    --prefix=/opt/fil --disable-static --docdir=/opt/fil/share/doc/acl-2.3.2
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf acl-2.3.2
hash -r

tar -xf $FILCSRC/pizlix/pcre2-10.44.tar.bz2
cd pcre2-10.44
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil \
            --enable-unicode                    \
            --disable-jit                       \
            --enable-pcre2-16                   \
            --enable-pcre2-32                   \
            --enable-pcre2grep-libz             \
            --enable-pcre2grep-libbz2           \
            --disable-static
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf pcre2-10.44
hash -r

tar -xf $FILCSRC/pizlix/ncurses-6.5.tar.gz
cd ncurses-6.5
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil \
    --mandir=/opt/fil/share/man \
    --with-shared \
    --without-debug \
    --without-normal \
    --without-ada \
    --with-cxx-shared \
    --enable-pc-files \
    --with-pkg-config-libdir=/opt/fil/lib/pkgconfig \
    --enable-overwrite
make -j `nproc`
make -j `nproc` DESTDIR=$PWD/dest install
install -vm755 dest/opt/fil/lib/libncursesw.so.6.5 /opt/fil/lib
rm -v dest/opt/fil/lib/libncursesw.so.6.5
sed -e 's/^#if.*XOPEN.*$/#if 1/' \
    -i dest/opt/fil/include/curses.h
cp -av dest/* /
for lib in ncurses form panel menu ; do
    ln -sfv lib${lib}w.so /opt/fil/lib/lib${lib}.so
    ln -sfv ${lib}w.pc /opt/fil/lib/pkgconfig/${lib}.pc
done
ln -sfv libncursesw.so /opt/fil/lib/libcurses.so
cp -v -R doc -T /opt/fil/share/doc/ncurses-6.5
cd ..
rm -rf ncurses-6.5
hash -r

tar -xf $FILCSRC/pizlix/readline-8.2.13.tar.gz
cd readline-8.2.13
sed -i '/MV.*old/d' Makefile.in
sed -i '/{OLDSUFF}/c:' support/shlib-install
sed -i 's/-Wl,-rpath,[^ ]*//' support/shobj-conf
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil \
    --disable-static \
    --with-curses \
    --docdir=/opt/fil/share/doc/readline-8.2.13
make -j `nproc` SHLIB_LIBS="-lncursesw"
make -j `nproc` SHLIB_LIBS="-lncursesw" install
install -v -m644 doc/*.{ps,pdf,html,dvi} /opt/fil/share/doc/readline-8.2.13
cd ..
rm -rf readline-8.2.13
hash -r

tar -xf $FILCSRC/projects/pkgconf-2.3.0/pizlonated-pkgconf.tar.gz
cd pizlonated-pkgconf
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil \
    --disable-static \
    --docdir=/opt/fil/share/doc/pkgconf-2.3.0
make -j `nproc`
make -j `nproc` install
ln -sv pkgconf /opt/fil/bin/pkg-config
ln -sv pkgconf.1 /opt/fil/share/man/man1/pkg-config.1
cd ..
rm -rf pizlonated-pkgconf
hash -r

tar -xf $FILCSRC/projects/libxcrypt-4.4.36/pizlonated-libxcrypt.tar.gz
cd pizlonated-libxcrypt
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil \
    --enable-hashes=strong,glibc \
    --enable-obsolete-api=no \
    --disable-static \
    --disable-failure-tokens
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf pizlonated-libxcrypt
hash -r

tar -xf $FILCSRC/projects/bash-5.2.32/pizlonated-bash.tar.gz
cd pizlonated-bash
CC="/opt/fil/bin/filcc -w" CXX="/opt/fil/bin/fil++ -w" ./configure --prefix=/opt/fil \
    --without-bash-malloc \
    --with-installed-readline \
    bash_cv_strtold_broken=no \
    --docdir=/opt/fil/share/doc/bash-5.2.32
make -j `nproc`
make -j `nproc` install
cd ..

test -d ../build
test -d ../../fil
rm -rf pizlonated-bash
hash -r

tar -xf $FILCSRC/projects/openssl-3.3.1/pizlonated-openssl.tar.gz
cd pizlonated-openssl
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./config --prefix=/opt/fil \
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
hash -r

tar -xf $FILCSRC/projects/libffi-3.4.6/pizlonated-libffi.tar.gz
cd pizlonated-libffi
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil \
    --disable-static \
    --with-gcc-arch=native \
    --disable-exec-static-tramp
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf pizlonated-libffi
hash -r

tar -xf $FILCSRC/pizlix/coreutils-9.5.tar.xz
cd coreutils-9.5
patch -Np1 -i $FILCSRC/pizlix/coreutils-9.5-i18n-2.patch
autoreconf -fiv
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ FORCE_UNSAFE_CONFIGURE=1 ./configure \
    --prefix=/opt/fil
make -j `nproc`
make -j `nproc` install
mv -v /opt/fil/bin/chroot /opt/fil/sbin
mkdir -pv /opt/fil/share/man/man8
mv -v /opt/fil/share/man/man1/chroot.1 /opt/fil/share/man/man8/chroot.8
sed -i 's/"1"/"8"/' /opt/fil/share/man/man8/chroot.8
cd ..
rm -rf coreutils-9.5
hash -r

tar -xf $FILCSRC/pizlix/mg-3.7.tar.gz 
cd mg-3.7
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf mg-3.7
hash -r

tar -xf $FILCSRC/projects/libuv-1.51.0/pizlonated-libuv.tar.gz
cd pizlonated-libuv
CC=/opt/fil/bin/filcc ./configure --prefix=/opt/fil
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf pizlonated-libuv
hash -r

tar -xf $FILCSRC/pizlix/audit-userspace-4.1.2.tar.gz
cd audit-userspace-4.1.2
autoreconf -f --install
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ CC_FOR_BUILD=/opt/fil/bin/filcc ./configure --prefix=/opt/fil --without-io_uring --without-python3 --disable-zos-remote
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf audit-userspace-4.1.2
hash -r

tar -xf $FILCSRC/projects/keyutils-1.6.3/pizlonated-keyutils.tar.gz
cd pizlonated-keyutils
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ make -j `nproc` NO_ARLIB=1 LIBDIR=/opt/fil/lib BINDIR=/opt/fil/bin SBINDIR=/opt/fil/sbin USRLIBDIR=/opt/fil/lib SHAREDIR=/opt/fil/share/keyutils INCLUDEDIR=/opt/fil/include PREFIX=/opt/fil install-optfil
cd ..
rm -rf pizlonated-keyutils
hash -r

tar -xf $FILCSRC/projects/libsepol-3.9/pizlonated-sepol.tar.gz
cd pizlonated-sepol
make -j `nproc` CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ DESTDIR=/opt/fil PREFIX=/
make -j `nproc` CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ DESTDIR=/opt/fil PREFIX=/ install
cd ..
rm -rf pizlonated-sepol
hash -r

tar -xf $FILCSRC/projects/libselinux-3.9/pizlonated-selinux.tar.gz
cd pizlonated-selinux
make -j `nproc` CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ DESTDIR=/opt/fil PREFIX=/
make -j `nproc` CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ DESTDIR=/opt/fil PREFIX=/ install
cd ..
rm -rf pizlonated-selinux
hash -r

tar -xf $FILCSRC/projects/Linux-PAM-1.7.1/pizlonated-pam.tar.gz
cd pizlonated-pam
mkdir -v build
cd build
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ meson \
    setup .. \
    --prefix=/opt/fil \
    --buildtype=release \
    -D openssl=enabled \
    -D read-both-confs=true \
    -D usergroups=true \
    -D nis=disabled \
    -D libdir=/opt/fil/lib \
    -D sysconfdir=/etc
ninja
ninja install
cd ../..
test -d pizlonated-pam
rm -rf pizlonated-pam
hash -r

tar -xf $FILCSRC/projects/dummy-pam-ecryptfs/pizlonated-dummy-pam-ecryptfs.tar.gz
cd pizlonated-dummy-pam-ecryptfs
make CC=/opt/fil/bin/filcc PREFIX=/opt/fil -j `nproc`
make CC=/opt/fil/bin/filcc PREFIX=/opt/fil -j `nproc` install
cd ..
rm -rf pizlonated-dummy-pam-ecryptfs
hash -r

tar -xf $FILCSRC/projects/krb5-1.21.3/pizlonated-krb5.tar.gz
cd pizlonated-krb5/src
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil --with-readline --with-crypto-impl=openssl --sysconfdir=/etc --localstatedir=/var/lib --runstatedir=/run
make -j `nproc`
make -j `nproc` install
cd ../..
test -d pizlonated-krb5
rm -rf pizlonated-krb5
hash -r

tar -xf $FILCSRC/projects/openssh-10.3p1/pizlonated-openssh.tar.gz
cd pizlonated-openssh
install -v -m700 -d /opt/fil/var/lib/sshd
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil \
            --sysconfdir=/etc/ssh \
            --with-privsep-path=/opt/fil/var/lib/sshd \
            --with-default-path=/opt/fil/bin:/usr/bin:/bin \
            --with-superuser-path=/opt/fil/sbin:/opt/fil/bin:/usr/sbin:/usr/bin:/bin \
            --with-pid-dir=/run \
            --with-pam \
            --with-kerberos5=/opt/fil
make -j `nproc`
make -j `nproc` install-nosysconf
make -j `nproc` sysconfdir=/opt/fil/share/examples/ssh install-sysconf 
install -v -m755    contrib/ssh-copy-id /opt/fil/bin
install -v -m644    contrib/ssh-copy-id.1 \
                    /opt/fil/share/man/man1
install -v -m755 -d /opt/fil/share/doc/openssh-10.3p1
install -v -m644    INSTALL LICENCE OVERVIEW README* \
                    /opt/fil/share/doc/openssh-10.3p1
cd ..
rm -rf pizlonated-openssh
hash -r

tar -xf $FILCSRC/projects/sudo-1.9.15p5/pizlonated-sudo.tar.gz
cd pizlonated-sudo
CC=/opt/fil/bin/filcc CC_FOR_BUILD=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil --sysconfdir=/etc --with-all-insults --with-pam --with-pam-login --with-fqdn --with-logging=syslog --with-logfac=authpriv --with-env-editor --with-editor=/opt/fil/bin/mg --with-timeout=15 --with-password-timeout=0 --with-passprompt="[memory safe sudo] password for %p: " --with-tty-tickets --without-sendmail --enable-zlib=system  --with-rundir=/opt/fil/var/sudo
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf pizlonated-sudo
hash -r

tar -xf $FILCSRC/projects/sed-4.9/pizlonated-sed.tar.gz
cd pizlonated-sed
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf pizlonated-sed
hash -r

tar -xf $FILCSRC/pizlix/psmisc-23.7.tar.xz
cd psmisc-23.7
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf psmisc-23.7
hash -r

tar -xf $FILCSRC/projects/m4-1.4.19/pizlonated-m4.tar.gz
cd pizlonated-m4
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf pizlonated-m4
hash -r

tar -xf $FILCSRC/pizlix/flex-2.6.4.tar.gz
cd flex-2.6.4
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil --disable-static
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf flex-2.6.4
hash -r

tar -xf $FILCSRC/projects/bison-3.8.2/pizlonated-bison.tar.gz
cd pizlonated-bison
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf pizlonated-bison
hash -r

tar -xf $FILCSRC/projects/grep-3.11/pizlonated-grep.tar.gz
cd pizlonated-grep
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf pizlonated-grep
hash -r

tar --no-same-owner -xf $FILCSRC/pizlix/less-661.tar.gz
cd less-661
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil --sysconfdir=/etc
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf less-661
hash -r

tar -xf $FILCSRC/projects/diffutils-3.10/pizlonated-diffutils.tar.gz
cd pizlonated-diffutils
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf pizlonated-diffutils
hash -r

tar -xf $FILCSRC/pizlix/gawk-5.3.0.tar.xz
cd gawk-5.3.0
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf gawk-5.3.0
hash -r

tar -xf $FILCSRC/pizlix/findutils-4.10.0.tar.xz
cd findutils-4.10.0
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf findutils-4.10.0
hash -r

tar -xf $FILCSRC/pizlix/gzip-1.13.tar.xz
cd gzip-1.13
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf gzip-1.13
hash -r

tar -xf $FILCSRC/projects/make-4.4.1/pizlonated-make.tar.gz
cd pizlonated-make
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf pizlonated-make
hash -r

tar -xf $FILCSRC/pizlix/patch-2.7.6.tar.xz
cd patch-2.7.6
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf patch-2.7.6
hash -r

tar -xf $FILCSRC/projects/tar-1.35/pizlonated-tar.tar.gz
cd pizlonated-tar
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ FORCE_UNSAFE_CONFIGURE=1 \
    ./configure --prefix=/opt/fil \
                --with-gzip=/opt/fil/bin/gzip \
                --with-bzip2=/opt/fil/bin/bzip2 \
                --with-lzma=/opt/fil/bin/lzma \
                --with-xz=/opt/fil/bin/xz \
                --with-zstd=/opt/fil/bin/zstd
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf pizlonated-tar
hash -r

tar -xf $FILCSRC/projects/icu-76.1/pizlonated-icu.tar.gz
cd pizlonated-icu/icu4c/source
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil
make -j `nproc`
make -j `nproc` install
cd ../../../
rm -rf pizlonated-icu
hash -r

tar -xf $FILCSRC/projects/procps-ng-4.0.4/pizlonated-procps.tar.gz
cd pizlonated-procps
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil --disable-static
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf pizlonated-procps
hash -r

tar -xf $FILCSRC/projects/libevent-2.1.12/pizlonated-libevent.tar.gz
cd pizlonated-libevent
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure  --prefix=/opt/fil --sysconfdir=/etc
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf pizlonated-libevent
hash -r

tar -xf $FILCSRC/projects/tmux-3.5a/pizlonated-tmux.tar.gz
cd pizlonated-tmux
CC="/opt/fil/bin/filcc -Wno-pointer-sign" CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil --sysconfdir=/etc
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf pizlonated-tmux
hash -r

tar -xf $FILCSRC/pizlix/libunistring-1.2.tar.xz
cd libunistring-1.2
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil --disable-static --sysconfdir=/etc
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf libunistring-1.2
hash -r

tar -xf $FILCSRC/projects/libidn2-2.3.7/pizlonated-libidn2.tar.gz
cd pizlonated-libidn2
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil --sysconfdir=/etc --disable-static
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf pizlonated-libidn2
hash -r

tar -xf $FILCSRC/pizlix/libpsl-0.21.5.tar.gz
cd libpsl-0.21.5
mkdir -v build
cd build
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ meson setup .. --prefix=/opt/fil --sysconfdir=/etc --buildtype=release
ninja
ninja install
cd ../..
rm -rf libpsl-0.21.5
hash -r

tar -xf $FILCSRC/projects/libtasn1-4.19.0/pizlonated-libtasn1.tar.gz
cd pizlonated-libtasn1
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil --sysconfdir=/etc --disable-static
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf pizlonated-libtasn1
hash -r

tar -xf $FILCSRC/projects/p11-kit-0.25.5/pizlonated-p11-kit.tar.gz
cd pizlonated-p11-kit
mkdir -v p11-build
cd p11-build
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ meson setup .. --prefix=/opt/fil --sysconfdir=/etc --buildtype=release
ninja
ninja install
cd ../..
rm -rf pizlonated-p11-kit
hash -r

tar -xf $FILCSRC/pizlix/nghttp2-1.62.1.tar.xz
cd nghttp2-1.62.1
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil --sysconfdir=/etc --disable-static --enable-lib-only
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf nghttp2-1.62.1
hash -r

tar -xf $FILCSRC/projects/curl-8.9.1/pizlonated-curl.tar.gz
cd pizlonated-curl
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil --sysconfdir=/etc --disable-static --with-openssl --enable-threaded-resolver --with-ca-path=/etc/ssl/certs --with-gsssapi
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf pizlonated-curl
hash -r

tar -xf $FILCSRC/pizlix/wget-1.24.5.tar.gz
cd wget-1.24.5
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil --sysconfdir=/etc --with-ssl=openssl
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf wget-1.24.5
hash -r

tar -xf $FILCSRC/projects/git-2.46.0/pizlonated-git.tar.gz
cd pizlonated-git
CC=/opt/fil/bin/filcc CXX=/opt/fil/bin/fil++ ./configure --prefix=/opt/fil --sysconfdir=/etc --with-libpcre2
make -j `nproc`
make -j `nproc` install
cd ..
rm -rf pizlonated-git
hash -r

cd ..
test -d build
test -d ../fil
rm -rf build

