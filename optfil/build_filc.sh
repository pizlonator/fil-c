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

test $EUID -ne 0

test "x$FILCSRC" != "x"
test -d $FILCSRC
test -d $FILCSRC/libpas
test -d $FILCSRC/llvm
test -d $FILCSRC/clang
test -d $FILCSRC/filc

# FIXME: It would be super cool to avoid building yolo glibc an extra time here. We do that only
# because I don't feel like fucking with the libpas build right now.

# FIXME: It would be super cool to build clang within the bootstrapping environment. But if we did
# that then the iteration time of this whole thing would be much worse. And it's totally unclear if
# that's even necessary.

# FIXME: This has a lot of copy pasta from the pizlix build.

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

./package-source.sh projects/libxcrypt-4.4.36 pizlonated-libxcrypt
./package-source.sh projects/xz-5.6.2 pizlonated-xz
./package-source.sh projects/pkgconf-2.3.0 pizlonated-pkgconf
./package-source.sh projects/bash-5.2.32 pizlonated-bash
./package-source.sh projects/openssl-3.3.1 pizlonated-openssl
./package-source.sh projects/libffi-3.4.6 pizlonated-libffi
./package-source.sh projects/sudo-1.9.15p5 pizlonated-sudo
./package-source.sh projects/openssh-9.8p1 pizlonated-openssh

test -d filc
test -d llvm
test -d clang
test -d libpas

rm -rf pizfix
. ./setup_glibc.sh
./configure_llvm.sh
./build_clang.sh

rm -rf pizfix/os-include
mkdir -p pizfix/os-include
pushd pizfix/os-include
ln -s ../../optfil/kernel-include/linux .
ln -s ../../optfil/kernel-include/asm .
ln -s ../../optfil/kernel-include/asm-generic .
popd

./build_yolo_glibc.sh
./build_runtime.sh
./build_user_glibc.sh
./build_cxx.sh
filc/run-tests

