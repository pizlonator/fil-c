#!/bin/bash
#
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
test -d $FILCSRC/projects

test $EUID -eq `stat -c %u $FILCSRC`

cd $FILCSRC

rm -vf projects/*/pizlonated-*.tar.gz

filc/projeny package \
    projects/xz.projeny projects/xz/pizlonated-xz.tar.gz \
    projects/m4.projeny projects/m4/pizlonated-m4.tar.gz \
    projects/pkgconf.projeny projects/pkgconf/pizlonated-pkgconf.tar.gz \
    projects/attr.projeny projects/attr/pizlonated-attr.tar.gz \
    projects/zunsafe-openssl3.projeny projects/zunsafe-openssl3/pizlonated-openssl.tar.gz \
    projects/libffi.projeny projects/libffi/pizlonated-libffi.tar.gz \
    projects/util-linux.projeny projects/util-linux/pizlonated-util-linux.tar.gz \
    projects/coreutils.projeny projects/coreutils/pizlonated-coreutils.tar.gz \
    projects/mg.projeny projects/mg/pizlonated-mg.tar.gz \
    projects/gzip.projeny projects/gzip/pizlonated-gzip.tar.gz \
    projects/patch.projeny projects/patch/pizlonated-patch.tar.gz \
    projects/zlib.projeny projects/zlib/pizlonated-zlib.tar.gz \
    projects/lz4.projeny projects/lz4/pizlonated-lz4.tar.gz

./package-source.sh projects/lfs-bootscripts-20240825 pizlonated-lfs-bootscripts
./package-source.sh projects/kbd-2.6.4 pizlonated-kbd
./package-source.sh projects/binutils-2.47 pizlonated-binutils
./package-source.sh projects/gmp-6.3.0 pizlonated-gmp
./package-source.sh projects/libxcrypt-4.5.2 pizlonated-libxcrypt
./package-source.sh projects/shadow-4.16.0 pizlonated-shadow
./package-source.sh projects/sed-4.10 pizlonated-sed
./package-source.sh projects/gettext-0.22.5 pizlonated-gettext
./package-source.sh projects/grep-3.12 pizlonated-grep
./package-source.sh projects/bash-5.3 pizlonated-bash
./package-source.sh projects/perl-5.40.0 pizlonated-perl
./package-source.sh projects/XML-Parser-2.47 pizlonated-xml-parser
./package-source.sh projects/elfutils-0.191 pizlonated-elfutils
./package-source.sh projects/Python-3.12.5 pizlonated-cpython
./package-source.sh projects/check-0.15.2 pizlonated-check
./package-source.sh projects/diffutils-3.12 pizlonated-diffutils
./package-source.sh projects/bison-3.8.2 pizlonated-bison
./package-source.sh projects/libpipeline-1.5.7 pizlonated-libpipeline
./package-source.sh projects/texinfo-7.1 pizlonated-texinfo
./package-source.sh projects/vim-9.1.0660 pizlonated-vim
./package-source.sh projects/systemd-256.4 pizlonated-systemd
./package-source.sh projects/procps-ng-4.0.7 pizlonated-procps
./package-source.sh projects/make-4.4.1 pizlonated-make
./package-source.sh projects/linux-6.10.5 pizlonated-linux
./package-source.sh projects/e2fsprogs-1.47.1 pizlonated-e2fsprogs
./package-source.sh projects/kmod-33 pizlonated-kmod
./package-source.sh projects/man-db-2.12.1 pizlonated-man-db
./package-source.sh projects/tar-1.35 pizlonated-tar
./package-source.sh projects/meson-1.5.1 pizlonated-meson
./package-source.sh projects/zstd-1.5.7 pizlonated-zstd

