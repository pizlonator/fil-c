#!/yolo/bin/bash

set -e
set -x

ulimit -c unlimited

cd /sources

test ! -e /usr/include
cp -rv /yolo/kernel-include /usr/include

tar -xf pizlonated-yolo-glibc.tar.gz
cd pizlonated-yolo-glibc
patch -Np1 -i ../glibc-2.40-fhs-1.patch
mkdir -v build
cd build
../configure --prefix=/usr --disable-mathvec --disable-nscd libc_cv_slibdir=/usr/lib
make
touch /etc/ld.so.conf
sed '/test-installation/s@$(PERL)@echo not running@' -i ../Makefile
make install
cd ../..
rm -rf pizlonated-yolo-glibc

