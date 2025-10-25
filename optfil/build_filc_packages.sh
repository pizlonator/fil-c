#!/bin/bash
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

test "x$FILCSRC" != "x"
test -d $FILCSRC
test -d $FILCSRC/libpas
test -d $FILCSRC/llvm
test -d $FILCSRC/clang
test -d $FILCSRC/filc
test -d $FILCSRC/optfil

FILCOWNER=`stat -c %U $FILCSRC`
test `id -u` -eq `id -u $FILCOWNER`

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
./package-source.sh projects/binutils-2.43.1 pizlonated-binutils
./package-source.sh projects/Linux-PAM-1.7.1 pizlonated-pam
./package-source.sh projects/keyutils-1.6.3 pizlonated-keyutils

