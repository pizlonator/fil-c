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

cd projects/yolo-glibc-2.44
git archive --format=tar HEAD --prefix=pizlonated-yolo-glibc/ | tar -xf -
git diff --relative HEAD . | (cd pizlonated-yolo-glibc && patch -p1)
cd pizlonated-yolo-glibc
autoconf
cd ..
tar -czf pizlonated-yolo-glibc.tar.gz pizlonated-yolo-glibc
rm -rf pizlonated-yolo-glibc
cd ../..

cd projects/user-glibc-2.44
git archive --format=tar HEAD --prefix=pizlonated-user-glibc/ | tar -xf -
git diff --relative HEAD . | (cd pizlonated-user-glibc && patch -p1)
cd pizlonated-user-glibc
autoconf
cd ..
tar -czf pizlonated-user-glibc.tar.gz pizlonated-user-glibc
rm -rf pizlonated-user-glibc
cd ../..

filc/projeny package \
    projects/xz.projeny projects/xz/pizlonated-xz.tar.gz \
    projects/pkgconf.projeny projects/pkgconf/pizlonated-pkgconf.tar.gz \
    projects/coreutils.projeny projects/coreutils/pizlonated-coreutils.tar.gz \
    projects/openssl.projeny projects/openssl/pizlonated-openssl.tar.gz \
    projects/libffi.projeny projects/libffi/pizlonated-libffi.tar.gz \
    projects/icu.projeny projects/icu/pizlonated-icu.tar.gz \
    projects/tmux.projeny projects/tmux/pizlonated-tmux.tar.gz \
    projects/libidn2.projeny projects/libidn2/pizlonated-libidn2.tar.gz \
    projects/m4.projeny projects/m4/pizlonated-m4.tar.gz \
    projects/xxHash.projeny projects/xxHash/pizlonated-xxHash.tar.gz \
    projects/rsync.projeny projects/rsync/pizlonated-rsync.tar.gz \
    projects/gzip.projeny projects/gzip/pizlonated-gzip.tar.gz \
    projects/attr.projeny projects/attr/pizlonated-attr.tar.gz \
    projects/libedit.projeny projects/libedit/pizlonated-libedit.tar.gz \
    projects/patchelf.projeny projects/patchelf/pizlonated-patchelf.tar.gz \
    projects/libxml2.projeny projects/libxml2/pizlonated-libxml2.tar.gz \
    projects/brotli.projeny projects/brotli/pizlonated-brotli.tar.gz \
    projects/blake3.projeny projects/blake3/pizlonated-blake3.tar.gz \
    projects/dash.projeny projects/dash/pizlonated-dash.tar.gz \
    projects/mg.projeny projects/mg/pizlonated-mg.tar.gz

./package-source.sh projects/libxcrypt-4.5.2 pizlonated-libxcrypt
./package-source.sh projects/bash-5.3 pizlonated-bash
./package-source.sh projects/openssh-10.5p1 pizlonated-openssh
./package-source.sh projects/binutils-2.47 pizlonated-binutils
./package-source.sh projects/Linux-PAM-1.7.2 pizlonated-pam
./package-source.sh projects/audit-userspace-4.2.1 pizlonated-audit
./package-source.sh projects/keyutils-1.6.3 pizlonated-keyutils
./package-source.sh projects/dummy-pam-ecryptfs pizlonated-dummy-pam-ecryptfs
./package-source.sh projects/krb5-1.22.2 pizlonated-krb5
./package-source.sh projects/libsepol-3.11 pizlonated-sepol
./package-source.sh projects/libselinux-3.11 pizlonated-selinux
./package-source.sh projects/sudo-1.9.17p2 pizlonated-sudo
filc/projeny package projects/libuv.projeny projects/libuv/pizlonated-libuv.tar.gz
./package-source.sh projects/sed-4.10 pizlonated-sed
./package-source.sh projects/bison-3.8.2 pizlonated-bison
./package-source.sh projects/grep-3.12 pizlonated-grep
./package-source.sh projects/diffutils-3.12 pizlonated-diffutils
./package-source.sh projects/make-4.4.1 pizlonated-make
./package-source.sh projects/tar-1.35 pizlonated-tar
./package-source.sh projects/procps-ng-4.0.7 pizlonated-procps
./package-source.sh projects/libtasn1-4.21.0 pizlonated-libtasn1
./package-source.sh projects/p11-kit-0.26.5 pizlonated-p11-kit
./package-source.sh projects/curl-8.22.0 pizlonated-curl
./package-source.sh projects/git-2.55.0 pizlonated-git
./package-source.sh projects/libevent-2.1.13 pizlonated-libevent
./package-source.sh projects/zstd-1.5.7 pizlonated-zstd
./package-source.sh projects/zip-3.0 pizlonated-zip
./package-source.sh projects/unzip-6.0 pizlonated-unzip
./package-source.sh projects/zsh-5.9.2 pizlonated-zsh

