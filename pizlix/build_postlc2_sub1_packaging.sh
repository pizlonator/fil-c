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
./package-source.sh projects/libtasn1-4.19.0 pizlonated-libtasn1
./package-source.sh projects/p11-kit-0.25.5 pizlonated-p11-kit
./package-source.sh projects/libidn2-2.3.7 pizlonated-libidn2
./package-source.sh projects/curl-8.9.1 pizlonated-curl
./package-source.sh projects/emacs-30.1 pizlonated-emacs
./package-source.sh projects/sudo-1.9.15p5 pizlonated-sudo
./package-source.sh projects/git-2.46.0 pizlonated-git
./package-source.sh projects/libuv-1.51.0 pizlonated-libuv
./package-source.sh projects/icu-76.1 pizlonated-icu
./package-source.sh projects/libxml2-2.14.4 pizlonated-libxml2
./package-source.sh projects/libarchive-3.7.4 pizlonated-libarchive
./package-source.sh projects/dhcpcd-10.0.8 pizlonated-dhcpcd
./package-source.sh projects/openssh-9.8p1 pizlonated-openssh
./package-source.sh projects/yaml-0.2.5 pizlonated-yaml
./package-source.sh projects/ruby-3.3.10 pizlonated-ruby
./package-source.sh projects/patchelf-0.18.0 pizlonated-patchelf

