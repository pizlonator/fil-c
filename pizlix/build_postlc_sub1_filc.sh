#!/bin/bash

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
test -d $FILCSRC/projects

cd $FILCSRC

rm -vf projects/pizlonated-*.tar.gz
./package-source.sh projects/lfs-bootscripts-20240825 pizlonated-lfs-bootscripts
./package-source.sh projects/kbd-2.6.4 pizlonated-kbd
./package-source.sh projects/xz-5.6.2 pizlonated-xz
./package-source.sh projects/m4-1.4.19 pizlonated-m4
./package-source.sh projects/pkgconf-2.3.0 pizlonated-pkgconf
./package-source.sh projects/binutils-2.43.1 pizlonated-binutils
./package-source.sh projects/gmp-6.3.0 pizlonated-gmp
./package-source.sh projects/attr-2.5.2 pizlonated-attr
./package-source.sh projects/libxcrypt-4.4.36 pizlonated-libxcrypt
./package-source.sh projects/shadow-4.16.0 pizlonated-shadow
./package-source.sh projects/sed-4.9 pizlonated-sed
./package-source.sh projects/gettext-0.22.5 pizlonated-gettext
./package-source.sh projects/grep-3.11 pizlonated-grep
./package-source.sh projects/bash-5.2.32 pizlonated-bash
./package-source.sh projects/perl-5.40.0 pizlonated-perl
./package-source.sh projects/XML-Parser-2.47 pizlonated-xml-parser
./package-source.sh projects/openssl-3.3.1 pizlonated-openssl
./package-source.sh projects/elfutils-0.191 pizlonated-elfutils
./package-source.sh projects/libffi-3.4.6 pizlonated-libffi
./package-source.sh projects/Python-3.12.5 pizlonated-cpython
./package-source.sh projects/check-0.15.2 pizlonated-check
./package-source.sh projects/diffutils-3.10 pizlonated-diffutils
./package-source.sh projects/bison-3.8.2 pizlonated-bison
./package-source.sh projects/libpipeline-1.5.7 pizlonated-libpipeline
./package-source.sh projects/texinfo-7.1 pizlonated-texinfo
./package-source.sh projects/vim-9.1.0660 pizlonated-vim
./package-source.sh projects/util-linux-2.40.2 pizlonated-util-linux
./package-source.sh projects/systemd-256.4 pizlonated-systemd
./package-source.sh projects/procps-ng-4.0.4 pizlonated-procps
./package-source.sh projects/make-4.4.1 pizlonated-make
./package-source.sh projects/linux-6.10.5 pizlonated-linux
./package-source.sh projects/e2fsprogs-1.47.1 pizlonated-e2fsprogs
./package-source.sh projects/kmod-33 pizlonated-kmod
./package-source.sh projects/man-db-2.12.1 pizlonated-man-db
./package-source.sh projects/tar-1.35 pizlonated-tar
./package-source.sh projects/meson-1.5.1 pizlonated-meson

