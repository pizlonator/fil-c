#!/bin/bash

set -e
set -x

ulimit -c unlimited

test "x$FILCSRC" != "x"
test -d $FILCSRC
test -d $FILCSRC/libpas
test -d $FILCSRC/llvm
test -d $FILCSRC/clang
test -d $FILCSRC/filc

test $EUID -eq `stat -c %u $FILCSRC`

export LFS=/mnt/lfs
test -d $LFS

# FIXME: It would be super cool to avoid building yolo glibc an extra time here. We do that only
# because I don't feel like fucking with the libpas build right now.

# FIXME: It would be super cool to build clang within the bootstrapping environment. But if we did
# that then the iteration time of this whole thing would be much worse. And it's totally unclear if
# that's even necessary.

# On the other hand, the cool thing about this approach is the we're testing the Fil-C runtime
# before proceeding to building the OS.
#
# But... there's the risk that the tests will poop themselves due to the use of (possibly
# incompatible) kernel's headers.

cd $FILCSRC

cd projects/yolo-glibc-2.40
git archive --format=tar HEAD --prefix=pizlonated-yolo-glibc/ | tar -xf -
git diff --relative HEAD . | (cd pizlonated-yolo-glibc && patch -p1)
cd pizlonated-yolo-glibc
autoconf
cd ..
tar -czf pizlonated-yolo-glibc.tar.gz pizlonated-yolo-glibc
rm -rf pizlonated-yolo-glibc
cd ../..

cd projects/user-glibc-2.40
git archive --format=tar HEAD --prefix=pizlonated-user-glibc/ | tar -xf -
git diff --relative HEAD . | (cd pizlonated-user-glibc && patch -p1)
cd pizlonated-user-glibc
autoconf
cd ..
tar -czf pizlonated-user-glibc.tar.gz pizlonated-user-glibc
rm -rf pizlonated-user-glibc
cd ../..

test -d filc
test -d llvm
test -d clang
test -d libpas

rm -rf pizfix
. ./setup_glibc.sh
./build_compiler_rt.sh
./build_yolounwind.sh
./configure_llvm.sh
./build_clang.sh

rm -rf pizfix/os-include
mkdir -p pizfix/os-include
pushd pizfix/os-include
ln -s $LFS/yolo/kernel-include/linux .
ln -s $LFS/yolo/kernel-include/asm .
ln -s $LFS/yolo/kernel-include/asm-generic .
popd

./build_yolo_glibc.sh
./build_runtime.sh
./build_user_glibc.sh
./build_cxx.sh
filc/run-tests

