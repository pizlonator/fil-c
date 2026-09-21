#!/bin/sh
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

set -e
set -x

# Source builds use the host architecture. package-build.sh embeds this script
# in setup.sh and passes the binary distribution's architecture explicitly.
target_arch=${1:-$(uname -m)}
case "$target_arch" in
    amd64) target_arch=x86_64 ;;
    arm64) target_arch=aarch64 ;;
    x86_64|aarch64) ;;
    *) echo "error: unsupported kernel header architecture: $target_arch" >&2; exit 1 ;;
esac

# Identify installed UAPI headers, not the running kernel: uname can describe
# the host of an emulated container, or be changed by a process personality.
# x86's 64-bit POSIX types and ARM64's ASIMD capability predate supported
# distributions. ARM32 also has hwcap.h, but does not define HWCAP_ASIMD.
# Reject mixed trees as well as an override for the wrong architecture.
asm_matches_target()
{
    case "$target_arch" in
        x86_64)
            test -r "$1/posix_types_64.h" && test ! -e "$1/hwcap.h"
            ;;
        aarch64)
            test ! -e "$1/posix_types_64.h" &&
                grep -Eq '^#[[:space:]]*define[[:space:]]+HWCAP_ASIMD[[:space:]]' "$1/hwcap.h" 2>/dev/null
            ;;
    esac
}

kernel_include=/usr/include
asm_include=
if test -n "${FILC_KERNEL_HEADERS:-}"
then
    # An explicit directory is useful for cross sysroots and nonstandard layouts.
    kernel_include=$FILC_KERNEL_HEADERS
    if test -d "$kernel_include"
    then
        kernel_include=$(CDPATH='' cd -- "$kernel_include" && pwd -P)
    fi
    asm_include=$kernel_include/asm
else
    if asm_matches_target /usr/include/asm
    then
        # Fedora, RHEL, openSUSE, and other native flat include layouts.
        asm_include=/usr/include/asm
    elif test -d /usr/include/"$target_arch"-linux-gnu/asm
    then
        # Debian/Ubuntu native multiarch layout.
        asm_include=/usr/include/$target_arch-linux-gnu/asm
    elif test -d /usr/"$target_arch"-linux-gnu/include/asm
    then
        # Keep all three directories from the same cross header package.
        kernel_include=/usr/$target_arch-linux-gnu/include
        asm_include=$kernel_include/asm
    fi
fi

for header_dir in "$asm_include" "$kernel_include/linux" "$kernel_include/asm-generic"
do
    if test ! -d "$header_dir" || test ! -f "$header_dir/types.h" ||
       test ! -s "$header_dir/types.h" || test ! -r "$header_dir/types.h"
    then
        echo "error: missing $target_arch kernel headers (${header_dir:-asm directory not found})." >&2
        echo "Each header directory must contain a readable, nonempty types.h file; check for missing files or broken symlinks." >&2
        echo "Install kernel-headers on Fedora/RHEL/Rocky, linux-libc-dev on Debian/Ubuntu, linux-api-headers on Arch, or linux-glibc-devel on openSUSE for native setup." >&2
        echo "For cross compilation, install the target's kernel headers (such as linux-libc-dev-arm64-cross or linux-libc-dev-amd64-cross on Debian/Ubuntu)." >&2
        echo "Or set FILC_KERNEL_HEADERS=/path/to/target/include to a directory containing asm, asm-generic, and linux, and rerun setup." >&2
        exit 1
    fi
done

if ! asm_matches_target "$asm_include"
then
    echo "error: missing $target_arch kernel headers (wrong or unrecognized asm architecture in $asm_include)." >&2
    echo "Set FILC_KERNEL_HEADERS to an include directory containing unmixed $target_arch asm, asm-generic, and linux headers, and rerun setup." >&2
    exit 1
fi

# Preflight all headers before changing existing links. Refuse real destination
# directories instead of creating links inside them.
mkdir -p pizfix/os-include
ln -sfnT "$asm_include" pizfix/os-include/asm
ln -sfnT "$kernel_include/linux" pizfix/os-include/linux
ln -sfnT "$kernel_include/asm-generic" pizfix/os-include/asm-generic
