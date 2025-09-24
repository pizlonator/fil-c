#!/bin/bash

set -e
set -x

echo Sub1: lfs user

ulimit -c unlimited

cd ~
pwd

set +h
umask 022
LFS=/mnt/lfs
LC_ALL=POSIX
LFS_TGT=$(uname -m)-lfs-linux-gnu
PATH=/usr/bin
if [ ! -L /bin ]; then PATH=/bin:$PATH; fi
PATH=$LFS/tools/bin:$PATH
CONFIG_SITE=$LFS/usr/share/config.site
export LFS LC_ALL LFS_TGT PATH CONFIG_SITE
export MAKEFLAGS=-j`nproc`

env

cd $LFS/sources

tar -xf binutils-2.43.1.tar.xz
cd binutils-2.43.1
mkdir -v build
cd build
../configure --prefix=$LFS/tools \
    --with-sysroot=$LFS \
    --target=$LFS_TGT \
    --disable-nls \
    --enable-gprofng=no \
    --disable-werror \
    --enable-new-dtags \
    --enable-default-hash-style=gnu \
    --disable-gprofng
make
make install
cd ../..
rm -rf binutils-2.43.1

# Idea: What if I did this shit:
#
# - Symlink /usr/include -> /yolo/include, /usr/lib -> /yolo/lib.
#
# - Build GCC without any special knowledge of /yolo other than making it the prefix.

tar -xf gcc-14.2.0.tar.xz
cd gcc-14.2.0
tar -xf ../mpfr-4.2.1.tar.xz
mv -v mpfr-4.2.1 mpfr
tar -xf ../gmp-6.3.0.tar.xz
mv -v gmp-6.3.0 gmp
tar -xf ../mpc-1.3.1.tar.gz
mv -v mpc-1.3.1 mpc
sed -e '/m64=/s/lib64/lib/' \
    -i.orig gcc/config/i386/t-linux64
mkdir -v build
cd build
../configure \
    --target=$LFS_TGT \
    --prefix=$LFS/tools \
    --with-glibc-version=2.40 \
    --with-sysroot=$LFS \
    --with-newlib \
    --without-headers \
    --enable-default-pie \
    --enable-default-ssp \
    --disable-nls \
    --disable-shared \
    --disable-multilib \
    --disable-threads \
    --disable-libatomic \
    --disable-libgomp \
    --disable-libquadmath \
    --disable-libssp \
    --disable-libvtv \
    --disable-libstdcxx \
    --enable-languages=c,c++
make
make install
cd ..
cat gcc/limitx.h gcc/glimits.h gcc/limity.h > \
    `dirname $($LFS_TGT-gcc -print-libgcc-file-name)`/include/limits.h
cd ..
rm -rf gcc-14.2.0

tar -xf linux-6.10.5.tar.xz
cd linux-6.10.5
make mrproper
make headers
find usr/include -type f ! -name '*.h' -delete
cp -rv usr/include $LFS/yolo
cp -rv usr/include $LFS/yolo/kernel-include
ln -sv ../yolo/include $LFS/usr/include
cd ..
rm -rf linux-6.10.5

# Option #2 (better but still meh?)
#
# We build yolo-glibc here, instead of normal glibc. And we make yolo glibc live in /yolo. And we put
# all of the binaries we build here in /yolo, including the gcc binary.
#
# The problem is: the GCC we build is not going to want to link to the yolo libraries!

tar -xf glibc-2.40.tar.xz
cd glibc-2.40
ln -sfv ../lib/ld-linux-x86-64.so.2 $LFS/lib64
ln -sfv ../lib/ld-linux-x86-64.so.2 $LFS/lib64/ld-lsb-x86-64.so.3
patch -Np1 -i ../glibc-2.40-fhs-1.patch
mkdir -v build
cd build
echo "rootsbindir=/yolo/sbin" > configparms
../configure \
    --prefix=/yolo \
    --host=$LFS_TGT \
    --build=$(../scripts/config.guess) \
    --enable-kernel=4.19 \
    --with-headers=$LFS/yolo/include \
    --disable-nscd \
    libc_cv_slibdir=/yolo/lib
make
make DESTDIR=$LFS install
sed '/RTLDLIST=/s@/yolo@@g' -i $LFS/yolo/bin/ldd
cd ../..
rm -rf glibc-2.40

tar -xf gcc-14.2.0.tar.xz
cd gcc-14.2.0
mkdir -v build
cd build
../libstdc++-v3/configure \
    --host=$LFS_TGT \
    --build=$(../config.guess) \
    --prefix=/yolo \
    --disable-multilib \
    --disable-nls \
    --disable-libstdcxx-pch \
    --with-gxx-include-dir=/tools/$LFS_TGT/include/c++/14.2.0
make
make DESTDIR=$LFS install
rm -v $LFS/yolo/lib/lib{stdc++{,exp,fs},supc++}.la
cd ../..
rm -rf gcc-14.2.0

# Option #1 (meh)
#
# Seems like at this point we have what it takes to run the Fil-C compiler, except that it'll be
# weird:
#
# - We could have run the Fil-C compiler even sooner if we had "just simply" configured it to use the
#   LFS mount directory as the place where it looks for its prefix.
#
# - It's not clear that we'd be using the right linker. It's not obvious that clang would know how to
#   use a linker that has the weird triple name.
#
# So, although this is one of the points where it's tempting to start using the Fil-C compiler, it
# might not be the right option.

tar -xf m4-1.4.19.tar.xz 
cd m4-1.4.19
./configure --prefix=/yolo \
    --host=$LFS_TGT \
    --build=$(build-aux/config.guess)
make
make DESTDIR=$LFS install
cd ..
rm -rf m4-1.4.19

tar -xf ncurses-6.5.tar.gz
cd ncurses-6.5
sed -i s/mawk// configure
mkdir -v build
cd build
../configure
make -C include
make -C progs tic
cd ..
./configure --prefix=/yolo \
    --host=$LFS_TGT \
    --build=$(./config.guess) \
    --mandir=/yolo/share/man \
    --with-manpage-format=normal \
    --with-shared \
    --without-normal \
    --with-cxx-shared \
    --without-debug \
    --without-ada \
    --disable-stripping \
    --enable-overwrite
make
make DESTDIR=$LFS TIC_PATH=$(pwd)/build/progs/tic install
ln -sv libncursesw.so $LFS/yolo/lib/libncurses.so
sed -e 's/^#if.*XOPEN.*$/#if 1/' \
    -i $LFS/yolo/include/curses.h
cd ..
rm -rf ncurses-6.5

tar -xf bash-5.2.32.tar.gz
cd bash-5.2.32
./configure --prefix=/yolo \
    --build=$(sh support/config.guess) \
    --host=$LFS_TGT \
    --without-bash-malloc \
    bash_cv_strtold_broken=no
make
make DESTDIR=$LFS install
ln -sv bash $LFS/bin/sh
cd ..
rm -rf bash-5.2.32

tar -xf coreutils-9.5.tar.xz
cd coreutils-9.5
./configure --prefix=/yolo \
    --host=$LFS_TGT \
    --build=$(build-aux/config.guess) \
    --enable-install-program=hostname \
    --enable-no-install-program=kill,uptime
make
make DESTDIR=$LFS install
mv -v $LFS/yolo/bin/chroot $LFS/yolo/sbin
mkdir -pv $LFS/yolo/share/man/man8
mv -v $LFS/yolo/share/man/man1/chroot.1 $LFS/yolo/share/man/man8/chroot.8
sed -i 's/"1"/"8"/' $LFS/yolo/share/man/man8/chroot.8
cd ..
rm -rf coreutils-9.5

tar -xf diffutils-3.10.tar.xz
cd diffutils-3.10
./configure --prefix=/yolo \
    --host=$LFS_TGT \
    --build=$(./build-aux/config.guess)
make
make DESTDIR=$LFS install
cd ..
rm -rf diffutils-3.10

tar -xf file-5.45.tar.gz
cd file-5.45
mkdir -v build
cd build
../configure --disable-bzlib \
    --disable-libseccomp \
    --disable-xzlib \
    --disable-zlib
make
cd ..
./configure --prefix=/yolo --host=$LFS_TGT --build=$(./config.guess)
make FILE_COMPILE=$(pwd)/build/src/file
make DESTDIR=$LFS install
rm -v $LFS/yolo/lib/libmagic.la
cd ..
rm -rf file-5.45

tar -xf findutils-4.10.0.tar.xz
cd findutils-4.10.0
./configure --prefix=/yolo \
    --localstatedir=/var/lib/locate \
    --host=$LFS_TGT \
    --build=$(build-aux/config.guess)
make
make DESTDIR=$LFS install
cd ..
rm -rf findutils-4.10.0

tar -xf gawk-5.3.0.tar.xz
cd gawk-5.3.0
sed -i 's/extras//' Makefile.in
./configure --prefix=/yolo \
    --host=$LFS_TGT \
    --build=$(build-aux/config.guess)
make
make DESTDIR=$LFS install
cd ..
rm -rf gawk-5.3.0

tar -xf grep-3.11.tar.xz
cd grep-3.11
./configure --prefix=/yolo \
    --host=$LFS_TGT \
    --build=$(./build-aux/config.guess)
make
make DESTDIR=$LFS install
cd ..
rm -rf grep-3.11

tar -xf gzip-1.13.tar.xz
cd gzip-1.13
./configure --prefix=/yolo --host=$LFS_TGT
make
make DESTDIR=$LFS install
cd ..
rm -rf gzip-1.13

tar -xf make-4.4.1.tar.gz
cd make-4.4.1
./configure --prefix=/yolo \
    --without-guile \
    --host=$LFS_TGT \
    --build=$(build-aux/config.guess)
make
make DESTDIR=$LFS install
cd ..
rm -rf make-4.4.1

tar -xf patch-2.7.6.tar.xz
cd patch-2.7.6
./configure --prefix=/yolo \
    --host=$LFS_TGT \
    --build=$(build-aux/config.guess)
make
make DESTDIR=$LFS install
cd ..
rm -rf patch-2.7.6

tar -xf sed-4.9.tar.xz
cd sed-4.9
./configure --prefix=/yolo \
    --host=$LFS_TGT \
    --build=$(./build-aux/config.guess)
make
make DESTDIR=$LFS install
cd ..
rm -rf sed-4.9

tar -xf tar-1.35.tar.xz
cd tar-1.35
./configure --prefix=/yolo \
    --host=$LFS_TGT \
    --build=$(build-aux/config.guess)
make
make DESTDIR=$LFS install
cd ..
rm -rf tar-1.35

tar -xf xz-5.6.2.tar.xz
cd xz-5.6.2
./configure --prefix=/yolo \
    --host=$LFS_TGT \
    --build=$(build-aux/config.guess) \
    --disable-static \
    --docdir=/yolo/share/doc/xz-5.6.2
make
make DESTDIR=$LFS install
rm -v $LFS/yolo/lib/liblzma.la
cd ..
rm -rf xz-5.6.2

tar -xf binutils-2.43.1.tar.xz
cd binutils-2.43.1
sed '6009s/$add_dir//' -i ltmain.sh
mkdir -v build
cd build
../configure \
    --prefix=/yolo \
    --build=$(../config.guess) \
    --host=$LFS_TGT \
    --disable-nls \
    --enable-shared \
    --enable-gprofng=no \
    --disable-werror \
    --enable-64-bit-bfd \
    --enable-new-dtags \
    --enable-default-hash-style=gnu \
    --disable-gprofng
make
make DESTDIR=$LFS install
rm -v $LFS/yolo/lib/lib{bfd,ctf,ctf-nobfd,opcodes,sframe}.{a,la}
cd ../..
rm -rf binutils-2.43.1

tar -xf gcc-14.2.0.tar.xz
cd gcc-14.2.0
tar -xf ../mpfr-4.2.1.tar.xz
mv -v mpfr-4.2.1 mpfr
tar -xf ../gmp-6.3.0.tar.xz
mv -v gmp-6.3.0 gmp
tar -xf ../mpc-1.3.1.tar.gz
mv -v mpc-1.3.1 mpc
sed -e '/m64=/s/lib64/lib/' \
    -i.orig gcc/config/i386/t-linux64
sed '/thread_header =/s/@.*@/gthr-posix.h/' \
    -i libgcc/Makefile.in libstdc++-v3/include/Makefile.in
mkdir -v build
cd build
../configure \
    --build=$(../config.guess) \
    --host=$LFS_TGT \
    --target=$LFS_TGT \
    LDFLAGS_FOR_TARGET=-L$PWD/$LFS_TGT/libgcc \
    --prefix=/yolo \
    --with-build-sysroot=$LFS \
    --enable-default-pie \
    --enable-default-ssp \
    --disable-nls \
    --disable-multilib \
    --disable-libatomic \
    --disable-libgomp \
    --disable-libquadmath \
    --disable-libsanitizer \
    --disable-libssp \
    --disable-libvtv \
    --enable-languages=c,c++
make
make DESTDIR=$LFS install
ln -sv gcc $LFS/yolo/bin/cc
cd ../..
rm -rf gcc-14.2.0

