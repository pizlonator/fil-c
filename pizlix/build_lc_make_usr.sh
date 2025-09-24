#!/bin/bash

set -e
set -x

test $EUID -eq 0
id -u lfs

export LFS=/mnt/lfs

test -d $LFS

mkdir -pv $LFS/usr
mkdir -v $LFS/usr/bin
mkdir -v $LFS/usr/lib
mkdir -v $LFS/usr/sbin

# We deliberately don't make /usr/include since the first step of build_lc_sub_chroot.sh is to
# install the kernel headers by creating /usr/include.

ln -sv /yolo/bin/bash $LFS/bin/sh
ln -sv /yolo/bin/bash $LFS/bin/bash
ln -sv /yolo/bin/m4 $LFS/usr/bin/m4 # This is awful, but is necessary because bison is stupid
ln -sv /yolo/bin/env $LFS/bin/env

mkdir -pv $LFS/lib/firmware

mkdir -pv $LFS/usr/{,local/}src
mkdir -pv $LFS/usr/local/include
mkdir -pv $LFS/usr/lib/locale
mkdir -pv $LFS/usr/local/{bin,lib,sbin}
mkdir -pv $LFS/usr/{,local/}share/{color,dict,doc,info,locale,man}
mkdir -pv $LFS/usr/{,local/}share/{misc,terminfo,zoneinfo}
mkdir -pv $LFS/usr/{,local/}share/man/man{1..8}

