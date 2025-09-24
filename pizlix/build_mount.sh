#!/bin/bash

set -e
set -x

export LFS=/mnt/lfs

mkdir -pv $LFS/{dev,proc,sys,run}
mount -v --bind /dev $LFS/dev
mount -vt devpts devpts -o gid=5,mode=0620 $LFS/dev/pts
mount -vt proc proc $LFS/proc
mount -vt sysfs sysfs $LFS/sys
mount -vt tmpfs tmpfs $LFS/run
mount -vt tmpfs -o nosuid,nodev tmpfs $LFS/dev/shm
