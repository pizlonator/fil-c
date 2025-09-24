#!/bin/bash

set -e
set -x

test $EUID -eq 0

SRCDIR=$PWD

./version-check.sh

export LFS=/mnt/lfs
export LFSDEV=`findmnt -nr -o SOURCE $LFS`

test "x$LFSDEV" != x
test "x$1" != x

./build_unmount.sh

cd $LFS
rm -rf *
tar -xpf $SRCDIR/lfs-$1.tar.gz

echo $1 Recovered
