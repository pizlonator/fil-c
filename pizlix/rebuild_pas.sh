#!/bin/bash

set -e
set -x

test $EUID -eq 0
id -u lfs

export LFS=/mnt/lfs

test -d $LFS
test -d $LFS/yolo/kernel-include

export FILCSRC=..
test -d $FILCSRC
test -d $FILCSRC/libpas

SRCDIR=$PWD

FILCOWNER=`stat -c %U $FILCSRC`
id -u $FILCOWNER
su $FILCOWNER ./rebuild_pas_sub_filc.sh

if test -n "$FILCLIB"
then
    echo "*******************************************************************************"
    echo
    echo "FILCLIB is set!"
    echo "Using $FILCSRC/pizfix/$FILCLIB/libpizlo.so"
    echo
    echo "*******************************************************************************"
else
    FILCLIB=lib
fi

test -e $FILCSRC/pizfix/$FILCLIB/libpizlo.so

cp -v $FILCSRC/pizfix/$FILCLIB/libpizlo.so $LFS/usr/lib/
cp -v $FILCSRC/pizfix/lib/filc_crt.o $LFS/usr/lib/
cp -v $FILCSRC/pizfix/lib/filc_mincrt.o $LFS/usr/lib/
test -d $LFS/usr/include
cp -v $FILCSRC/pizfix/stdfil-include/*.h $LFS/usr/include/
