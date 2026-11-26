#!/bin/bash
#
# Copyright (c) 2025 Epic Games, Inc. All Rights Reserved.
# Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
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
# THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# make_bootable_usb.sh -- write a Pizlix OS image directly to a USB stick.
#
# Like make_disk_image.sh, but instead of creating a disk.img file it
# partitions, formats, and populates a physical USB device.  ALL DATA ON
# THE TARGET DEVICE IS DESTROYED.
#
# Uses UEFI boot (per usb_stick_changes.txt §2): partition 1 is a 512 MiB
# EFI System Partition (ESP) with a FAT32 filesystem, and grub is installed
# with --target=x86_64-efi --removable so UEFI firmware boots it
# unconditionally from removable media.
#
# Usage:  make_bootable_usb.sh <usb-device> [tarball]
#   usb-device : whole-disk block device, e.g. /dev/sda
#   tarball    : rootfs tarball (default: lfs-postlc5.tar.gz, or -- to skip)

set -e
set -x

ulimit -c unlimited

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------

if test -z "$1"
then
    echo "Usage: $0 <usb-device> [tarball]" >&2
    echo "  usb-device : whole-disk block device to write to (e.g. /dev/sda)" >&2
    echo "  tarball    : rootfs tarball (default: lfs-postlc5.tar.gz, or -- to skip)" >&2
    exit 1
fi

DEVICE=$1

if test -z "$2"
then
    TARBALL=lfs-postlc5.tar.gz
else
    TARBALL=$2
fi

# ---------------------------------------------------------------------------
# Sanity checks
# ---------------------------------------------------------------------------

# Must run as root.
test $EUID -eq 0

# Device must exist and be a block device.
test -b "$DEVICE"

# Resolve symlinks (e.g. /dev/disk/by-id/usb-* -> /dev/sda).
DEVICE=$(readlink -f "$DEVICE")

# Extract the kernel device name (e.g. "sda" from "/dev/sda").
DEVNAME=$(basename "$DEVICE")

# --- Defense: must be a whole disk, not a partition -----------------------

if ! test -d "/sys/block/$DEVNAME"
then
    echo "Error: $DEVICE does not appear to be a whole-disk device" >&2
    echo "       (not found in /sys/block/$DEVNAME)." >&2
    echo "       Did you specify a partition (e.g. ${DEVICE}1) by mistake?" >&2
    exit 1
fi

# --- Defense: device must not be mounted or in use as swap ----------------

# Check for mount points via lsblk (covers the device and all its partitions).
if lsblk -ln -o MOUNTPOINT "$DEVICE" 2>/dev/null | grep -q '^/'
then
    echo "Error: $DEVICE (or one of its partitions) is currently mounted." >&2
    echo "       Unmount it first, e.g.:" >&2
    echo "         umount ${DEVICE}?*" >&2
    exit 1
fi

# Belt-and-suspenders: also check /proc/mounts directly.
if grep -q "^${DEVICE}" /proc/mounts 2>/dev/null
then
    echo "Error: $DEVICE (or one of its partitions) is listed in /proc/mounts." >&2
    echo "       Unmount it first." >&2
    exit 1
fi

# Check for swap usage.
if grep -q "^${DEVICE}" /proc/swaps 2>/dev/null
then
    echo "Error: $DEVICE (or one of its partitions) is in use as swap." >&2
    echo "       Disable it first, e.g.:" >&2
    echo "         swapoff ${DEVICE}?*" >&2
    exit 1
fi

# --- Defense: device must be a removable USB stick ------------------------

# Check 1: the removable flag in sysfs.
REMOVABLE=$(cat "/sys/block/$DEVNAME/removable" 2>/dev/null || echo "")
if [ "$REMOVABLE" != "1" ]
then
    echo "Error: $DEVICE is not marked as removable" >&2
    echo "       (removable=${REMOVABLE:-<unknown>})." >&2
    echo "       Refusing to write to a non-removable device." >&2
    exit 1
fi

# Check 2: transport must be USB (via lsblk).
TRAN=$(lsblk -dn -o TRAN "$DEVICE" 2>/dev/null | tr -d '[:space:]')
if [ -z "$TRAN" ]
then
    echo "Error: could not determine the transport type of $DEVICE." >&2
    echo "       Refusing to write to an unknown device type." >&2
    exit 1
fi
if [ "$TRAN" != "usb" ]
then
    echo "Error: $DEVICE uses transport '$TRAN', not 'usb'." >&2
    echo "       Refusing to write to a non-USB device." >&2
    exit 1
fi

# Check 3: confirm via udevadm that the bus is USB.
BUS=$(udevadm info --query=property --name="$DEVICE" 2>/dev/null \
      | grep '^ID_BUS=' | cut -d= -f2)
if [ -z "$BUS" ]
then
    echo "Error: could not determine the bus type of $DEVICE via udevadm." >&2
    echo "       Refusing to write to an unknown device type." >&2
    exit 1
fi
if [ "$BUS" != "usb" ]
then
    echo "Error: $DEVICE is on bus '$BUS', not 'usb'." >&2
    echo "       Refusing to write to a non-USB device." >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Pre-flight checks: tools, files, and device size
# (all verified BEFORE the destructive countdown so we never wipe the
#  stick only to discover a missing dependency)
# ---------------------------------------------------------------------------

# Required tools must all be present.
for t in parted partprobe mkfs.ext4 mkfs.vfat mkswap grub-install udevadm lsblk wipefs
do
    command -v "$t" >/dev/null 2>&1 || {
        echo "Error: required tool '$t' not found." >&2
        exit 1
    }
done

# grub UEFI modules must be installed.
test -d /usr/lib/grub/x86_64-efi || {
    echo "Error: grub-efi-amd64-bin not installed (no /usr/lib/grub/x86_64-efi)." >&2
    exit 1
}

# Tarball must exist (unless -- was given).
if test "$TARBALL" != "--"
then
    test -e "$TARBALL"
fi

# Working directory must be the pizlix directory.
test -d ..
test -d ../projects
test -d ../llvm
test -d etc
test -e LFS-12.2-SYSV-BOOK.pdf
test -e usb_fstab
test -e usb_grub_config

# Device must be large enough (root partition starts at 30 GiB).
DEV_SIZE=$(lsblk -dnbo SIZE "$DEVICE" 2>/dev/null)
MIN_SIZE=$((32 * 1024 * 1024 * 1024))
if [ -z "$DEV_SIZE" ] || [ "$DEV_SIZE" -lt "$MIN_SIZE" ]
then
    echo "Error: $DEVICE is too small (need at least 32 GiB," >&2
    echo "       got ${DEV_SIZE:-unknown} bytes)." >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Cleanup / defer mechanism
# ---------------------------------------------------------------------------

CLEANUP=""
defer() {
    CLEANUP="$1; $CLEANUP"
    trap "set +e; $CLEANUP" EXIT
}

# ---------------------------------------------------------------------------
# Prepare mount points
# ---------------------------------------------------------------------------

umount image-mount 2>/dev/null || echo whatever
rm -rf image-mount
mkdir image-mount

umount grub-mount 2>/dev/null || echo whatever
rm -rf grub-mount
mkdir grub-mount

# ---------------------------------------------------------------------------
# Wipe existing signatures and partition the device
# (UEFI layout per usb_stick_changes.txt §2: partition 1 is a 512 MiB ESP)
# ---------------------------------------------------------------------------

wipefs -a "$DEVICE" 2>/dev/null || true

parted "$DEVICE" <<EOF
mklabel gpt
mkpart ESP fat32 1MiB 512MiB
set 1 esp on
mkpart swap 512MiB 30GiB
mkpart root 30GiB 100%
EOF

partprobe "$DEVICE" 2>/dev/null || true
udevadm settle

# Verify that the kernel sees all three partitions (with retry for slow USB devices).
PARTITIONS_READY=false
for i in 1 2 3 4 5
do
    if test -e "${DEVICE}1" && test -e "${DEVICE}2" && test -e "${DEVICE}3"
    then
        PARTITIONS_READY=true
        break
    fi
    sleep 1
done

if [ "$PARTITIONS_READY" != "true" ]
then
    echo "Error: partition nodes for $DEVICE did not appear after 5 seconds." >&2
    echo "       Partitions ${DEVICE}1..3 should exist but at least one is missing." >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Create filesystems
# (partition 1 is a FAT32 ESP; partition 3 is ext4 root; -F forces overwrite)
# ---------------------------------------------------------------------------

mkfs.ext4 -F -L root "${DEVICE}3"
mkfs.vfat -F32 -n PIZLIX_ESP "${DEVICE}1"
mkswap -L swap "${DEVICE}2"

# ---------------------------------------------------------------------------
# Mount and populate
# ---------------------------------------------------------------------------

mount "${DEVICE}3" image-mount
defer "umount image-mount"

mount "${DEVICE}1" grub-mount
defer "umount grub-mount"

if test "$TARBALL" != "--"
then
    tar -xf "$TARBALL" -C image-mount
fi

# ---------------------------------------------------------------------------
# Install grub configuration on the ESP (from usb_grub_config)
# ---------------------------------------------------------------------------

mkdir -p grub-mount/EFI/BOOT grub-mount/boot/grub
cp usb_grub_config grub-mount/boot/grub/grub.cfg

# ---------------------------------------------------------------------------
# Overwrite /etc/fstab in the root partition (from usb_fstab)
# ---------------------------------------------------------------------------

mkdir -p image-mount/etc
cp usb_fstab image-mount/etc/fstab

# ---------------------------------------------------------------------------
# Install grub to the device (UEFI, per usb_stick_changes.txt §2)
# ---------------------------------------------------------------------------

grub-install --target=x86_64-efi --efi-directory=grub-mount \
    --boot-directory=grub-mount/boot --removable --no-nvram \
    --modules="part_gpt fat ext2" "$DEVICE"

# ---------------------------------------------------------------------------
# Flush everything to the USB stick
# ---------------------------------------------------------------------------

sync

echo ""
echo "Done!  Pizlix has been written to $DEVICE."
echo "You can now safely remove the USB stick (sync/eject) and boot from it."
