#!/bin/bash

set -e
set -x

export LFS=/mnt/lfs

umount $LFS/dev/shm || echo whatever
umount $LFS/dev/pts || echo whatever
umount $LFS/{sys,proc,run,dev} || echo whatever
