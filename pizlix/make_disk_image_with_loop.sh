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

TARBALL=$1
LOOP=$2

test -n "$TARBALL"
test -n "$LOOP"
test -e $TARBALL
test -e $LOOP
ls ${LOOP}*
test -e ${LOOP}p1
test -e ${LOOP}p2
test -e ${LOOP}p3
test -e ${LOOP}p4
test -d ..
test -d ../projects
test -d ../llvm
test -d etc
test -e LFS-12.2-SYSV-BOOK.pdf

umount image-mount || echo whatever
rm -rf image-mount
mkdir image-mount

mkfs.ext4 -L root ${LOOP}p4
mkswap -L swap ${LOOP}p3

mount ${LOOP}p4 image-mount
tar -xf $TARBALL -C image-mount

mkdir image-mount/boot/grub

cat > image-mount/boot/grub/grub.cfg <<EOF
set timeout=5
set default=0

menuentry "Pizlix" {
   set root=(hd0,4)
   linux /boot/vmlinuz-6.10.5-lfs-12.2 root=/dev/sda4 ro console=ttyS0,115200 console=tty0 net.ifnames=0
}
EOF

grub-install --target=i386-pc --boot-directory=image-mount/boot --modules="part_gpt ext2" $LOOP

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

