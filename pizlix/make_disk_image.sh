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

if test "$TARBALL" != "--"
then
    test -e $TARBALL
fi

test $EUID -eq 0
test -d ..
test -d ../projects
test -d ../llvm
test -d etc
test -e LFS-12.2-SYSV-BOOK.pdf

CLEANUP=""
defer() {
    CLEANUP="$1; $CLEANUP"
    trap "set +e; $CLEANUP" EXIT
}

test ! -d disk.vmdk.lck
mkdir disk.vmdk.lck
defer "rmdir disk.vmdk.lck"

rm -f disk.img
truncate -s 100G disk.img

parted disk.img <<EOF
mklabel gpt
mkpart bios_boot 1MiB 2MiB
set 1 bios_grub on
mkpart grub 2MiB 20MiB
mkpart swap 20MiB 30GiB
mkpart root 30GiB 100%
EOF

LOOP=$(losetup -Pf --show disk.img)
defer "losetup -d $LOOP"
udevadm settle

test -e $LOOP
test -e ${LOOP}p1
test -e ${LOOP}p2
test -e ${LOOP}p3
test -e ${LOOP}p4

umount image-mount || echo whatever
rm -rf image-mount
mkdir image-mount
umount grub-mount || echo whatever
rm -rf grub-mount
mkdir grub-mount

mkfs.ext4 -L root ${LOOP}p4
mkfs.ext4 -L root ${LOOP}p2
mkswap -L swap ${LOOP}p3

mount ${LOOP}p4 image-mount
defer "umount image-mount"
mount ${LOOP}p2 grub-mount
defer "umount grub-mount"

if test "$TARBALL" != "--"
then
    tar -xf $TARBALL -C image-mount
fi

mkdir -p grub-mount/boot/grub
cp etc/grub.cfg grub-mount/boot/grub/grub.cfg

grub-install --target=i386-pc --boot-directory=grub-mount/boot --modules="part_gpt ext2" $LOOP

cat > disk.vmdk <<EOF
# Disk DescriptorFile
version=1
CID=fffffffe
parentCID=ffffffff
createType="monolithicFlat"

RW $(($(stat -c%s disk.img) / 512)) FLAT "disk.img" 0
EOF

FILCOWNER=`stat -c %U ..`
FILCGROUP=`stat -c %G ..`
id -u $FILCOWNER
getent group $FILCGROUP
chown $FILCOWNER:$FILCGROUP disk.img disk.vmdk

