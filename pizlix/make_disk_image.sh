#!/bin/bash
#
# Copyright (c) 2025 Epic Games, Inc. All Rights Reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY EPIC GAMES, INC. ``AS IS AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL EPIC GAMES, INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

set -e
set -x

ulimit -c unlimited

if test -z "$1"
then
    TARBALL=lfs-postlc5.tar.gz
else
    TARBALL=$1
fi

test -e $TARBALL

test $EUID -eq 0
test -d ..
test -d ../projects
test -d ../llvm
test -d etc
test -e LFS-12.2-SYSV-BOOK.pdf

test ! -d disk.vmdk.lck
mkdir disk.vmdk.lck

rm -f disk.img
truncate -s 100G disk.img

parted disk.img <<EOF
mklabel gpt
mkpart bios_boot 1MiB 2MiB
set 1 bios_grub on
mkpart dummy 2MiB 3MiB
mkpart swap 3MiB 30GiB
mkpart root 30GiB 100%
EOF

LOOP=$(losetup -Pf --show disk.img)
(./make_disk_image_with_loop.sh $TARBALL $LOOP && RESULT=0) || RESULT=1
umount image-mount || echo failed to unmount
losetup -d $LOOP || echo failed to delete loop
rmdir disk.vmdk.lck
exit $RESULT

