#!/bin/bash

set -e
set -x

ulimit -c unlimited

test "x$FILCSRC" != "x"
test -d $FILCSRC
test -d $FILCSRC/projects

test $EUID -eq `stat -c %u $FILCSRC`

cd $FILCSRC

rm -vf projects/*/pizlonated-*.tar.gz
./package-source.sh projects/cmake-3.30.2 pizlonated-cmake
./package-source.sh projects/libtasn1-4.21.0 pizlonated-libtasn1
./package-source.sh projects/p11-kit-0.26.5 pizlonated-p11-kit
filc/projeny package projects/libidn2.projeny projects/libidn2/pizlonated-libidn2.tar.gz
./package-source.sh projects/curl-8.22.0 pizlonated-curl
./package-source.sh projects/emacs-30.1 pizlonated-emacs
./package-source.sh projects/sudo-1.9.17p2 pizlonated-sudo
./package-source.sh projects/git-2.55.0 pizlonated-git
./package-source.sh projects/libuv-1.52.1 pizlonated-libuv
filc/projeny package projects/libxml2.projeny projects/libxml2/pizlonated-libxml2.tar.gz
filc/projeny package projects/icu.projeny projects/icu/pizlonated-icu.tar.gz
./package-source.sh projects/libarchive-3.7.4 pizlonated-libarchive
./package-source.sh projects/dhcpcd-10.0.8 pizlonated-dhcpcd
./package-source.sh projects/openssh-10.5p1 pizlonated-openssh
./package-source.sh projects/yaml-0.2.5 pizlonated-yaml
./package-source.sh projects/ruby-3.3.10 pizlonated-ruby
filc/projeny package projects/patchelf.projeny projects/patchelf/pizlonated-patchelf.tar.gz

