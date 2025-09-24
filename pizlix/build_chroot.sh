#!/bin/bash

set -e
set -x

export LFS=/mnt/lfs

test "x$*" != x

chroot "$LFS" /yolo/bin/env -i \
    HOME=/root \
    TERM="$TERM" \
    PS1='(lfs chroot) \u:\w\$ ' \
    PATH=/usr/bin:/usr/sbin:/yolo/bin:/yolo/sbin \
    MAKEFLAGS="-j$(nproc)" \
    TESTSUITEFLAGS="-j$(nproc)" \
    "$@"
