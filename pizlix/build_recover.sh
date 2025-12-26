#!/bin/bash

set -e
set -x

test $EUID -eq 0

SRCDIR=$PWD

export LFS=/mnt/lfs

test "x$1" != x

./build_unmount.sh

cd $LFS
rm -rf *
tar -xpf $SRCDIR/lfs-$1.tar.gz

echo $1 Recovered
