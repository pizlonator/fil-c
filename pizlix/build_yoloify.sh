#!/bin/bash

set -e
set -x

ulimit -c unlimited

test $EUID -eq 0
id -u lfs

export LFS=/mnt/lfs

test -d $LFS

rm $LFS/usr/bin
rm $LFS/usr/lib
rm $LFS/usr/include
rm $LFS/usr/sbin

for binary in $LFS/yolo/bin/* $LFS/yolo/sbin/* $LFS/yolo/libexec/gcc/x86_64-lfs-linux-gnu/14.2.0/*
do
    if test ! -L $binary && test -x $binary
    then
        patchelf --set-rpath /yolo/lib $binary || echo whatever
        patchelf --set-interpreter /yolo/lib/ld-linux-x86-64.so.2 $binary || echo whatever
    fi
done

patchelf --set-rpath /yolo/lib/perl5/5.40/core_perl/CORE:/yolo/lib $LFS/yolo/bin/perl
patchelf --set-rpath /yolo/lib/perl5/5.40/core_perl/CORE:/yolo/lib $LFS/yolo/bin/perl5.40.0

sed -i '/\/usr\/bin\/perl/s//\/yolo\/bin\/perl/' \
    $LFS/yolo/bin/makeinfo \
    $LFS/yolo/bin/corelist \
    $LFS/yolo/bin/instmodsh \
    $LFS/yolo/bin/json_pp \
    $LFS/yolo/bin/pod2texi \
    $LFS/yolo/bin/prove \
    $LFS/yolo/bin/ptar \
    $LFS/yolo/bin/ptardiff \
    $LFS/yolo/bin/ptargrep \
    $LFS/yolo/bin/streamzip \
    $LFS/yolo/bin/texi2any \
    $LFS/yolo/bin/zipdetails

