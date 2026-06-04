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

rm -vf projects/*/pizlonated-*.tar.gz

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
./package-source.sh projects/openssh-10.3p1 pizlonated-openssh
./package-source.sh projects/binutils-2.43.1 pizlonated-binutils
./package-source.sh projects/Linux-PAM-1.7.1 pizlonated-pam
./package-source.sh projects/keyutils-1.6.3 pizlonated-keyutils
./package-source.sh projects/dummy-pam-ecryptfs pizlonated-dummy-pam-ecryptfs
./package-source.sh projects/krb5-1.21.3 pizlonated-krb5
./package-source.sh projects/libsepol-3.9 pizlonated-sepol
./package-source.sh projects/libselinux-3.9 pizlonated-selinux
./package-source.sh projects/sudo-1.9.15p5 pizlonated-sudo
./package-source.sh projects/libuv-1.51.0 pizlonated-libuv
./package-source.sh projects/sed-4.9 pizlonated-sed
./package-source.sh projects/bison-3.8.2 pizlonated-bison
./package-source.sh projects/grep-3.11 pizlonated-grep
./package-source.sh projects/diffutils-3.10 pizlonated-diffutils
./package-source.sh projects/make-4.4.1 pizlonated-make
./package-source.sh projects/tar-1.35 pizlonated-tar
./package-source.sh projects/icu-76.1 pizlonated-icu
./package-source.sh projects/procps-ng-4.0.4 pizlonated-procps
./package-source.sh projects/tmux-3.5a pizlonated-tmux
./package-source.sh projects/libidn2-2.3.7 pizlonated-libidn2
./package-source.sh projects/libtasn1-4.19.0 pizlonated-libtasn1
./package-source.sh projects/p11-kit-0.25.5 pizlonated-p11-kit
./package-source.sh projects/curl-8.9.1 pizlonated-curl
./package-source.sh projects/git-2.46.0 pizlonated-git
./package-source.sh projects/libevent-2.1.12 pizlonated-libevent
./package-source.sh projects/m4-1.4.19 pizlonated-m4
./package-source.sh projects/zstd-1.5.6 pizlonated-zstd

