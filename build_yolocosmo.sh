#!/bin/sh
#
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
# THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
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

# Builds the yolo (un-pizlonated) cosmopolitan libc that sits below libpizlo
# in the cosmo flavor of Fil-C, for every supported target architecture
# (COSMOARCHES, default "x86_64 aarch64"), and installs it into pizfix
# together with the APE bootloader bits, the APE linker tooling, the cross
# builtins/unwinder, the cross libpas runtime, and the yolo-include headers.
#
# Per-architecture artifacts land in pizfix/lib (x86_64, the flavor marker
# pizfix/lib/libyolocosmo.a lives there) and pizfix/lib-aarch64 (aarch64).
# The cosmo headers are arch-neutral, so pizfix/yolo-include stays shared.
#
# Only a slim subset of cosmo is built: the objects whose symbols the Fil-C
# runtime (libpizlo.a), the yolo crt chain, the pizlonated user libc, and the
# pizlonated C++ runtimes can reference.  That subset is enumerated in
# projects/yolocosmo/filc/filc-objects.mk (x86_64) and
# projects/yolocosmo/filc/filc-objects-aarch64.mk, which
# projects/yolocosmo/filc/compute-closure.py regenerates from the full
# cosmopolitan.a when cosmo or the runtime changes (see that script for the
# exact method).  zlib, ncurses, mbedtls, the cosmo test/tool trees, etc. are
# not built and not installed.
#
# The exception is the APE linker tooling (the `filcapetools` make target):
# apelink and pecheck are full-blown cosmo programs that the Fil-C clang
# driver runs at link time, so they are built in the Linux-only x86_64-
# optlinux mode (they only ever run on the Linux build host).  They are
# installed as pizfix/libexec/apelink and pizfix/libexec/pecheck, next to
# the Apple-silicon loader source that apelink embeds, as
# pizfix/libexec/ape-m1.c, and the per-architecture APE loaders that apelink
# embeds, installed as pizfix/lib/ape-x86_64.elf and
# pizfix/lib-aarch64/ape-aarch64.elf (each built in the plain <arch> mode,
# exactly like cosmo's own cosmocc packaging ships them, since the loaders
# have to run on every OS).

. libpas/common.sh

set -e
set -x

# The cosmo modes to build. <arch>-ape is the mode with the optimizations of
# <arch>-optlinux but the full OS support vector (see
# projects/yolocosmo/build/config.mk): on x86_64 the full vector is what
# compiles in the APE header machinery (ape.S's PE/Mach-O/BSD blobs and
# WinMain) that apelink needs to turn every cosmo-mode link into a real APE;
# on aarch64 Windows/Metal fold away and it is apelink that generates every
# header.
COSMOARCHES=${COSMOARCHES:-"x86_64 aarch64"}

# Build cosmo. This is a pure GNU-make build (no ./configure); the first run
# downloads its own GCC 14.1 toolchain into projects/yolocosmo/.cosmocc. The
# 'filcyolo' target builds the slim libyolocosmo.a (see above), the APE
# bootloader bits, and libc/crt/crt.o without running cosmo's own test suite.
# The 'filcapetools' target builds the APE linker tooling (see above); it
# recursively re-invokes make in the x86_64 and plain modes, so it needs its
# own make run.
cd projects/yolocosmo

for COSMOARCH in $COSMOARCHES
do
    $MAKE -j $NCPU m=$COSMOARCH-ape filcyolo
done

$MAKE -j $NCPU filcapetools

cd ../..

mkdir -p pizfix/lib pizfix/lib-aarch64 pizfix/libexec

install_yolocosmo()
{
    ARCH=$1
    LIB=pizfix/lib
    if test "x$ARCH" != "xx86_64"
    then
        LIB=pizfix/lib-$ARCH
    fi
    MODE=$ARCH-ape

    # The cosmo flavor marker file. Its existence in pizfix/lib (or
    # pizfix/lib-aarch64) flips the libpas Makefile into cosmo mode for that
    # architecture (COSMO != empty).
    cp -f projects/yolocosmo/o/$MODE/filc/libyolocosmo.a $LIB/libyolocosmo.a
    if test "x$ARCH" != "xx86_64"
    then
        # aarch64: cosmo's own linker script name, and no APE header object
        # (apelink generates every header; see the Makefile's filcyolo).
        cp -f projects/yolocosmo/o/$MODE/ape/aarch64.lds $LIB/aarch64.lds
    else
        cp -f projects/yolocosmo/o/$MODE/ape/ape.o pizfix/lib/ape.o
        cp -f projects/yolocosmo/o/$MODE/ape/ape.lds pizfix/lib/ape.lds
    fi
    cp -f projects/yolocosmo/o/$MODE/libc/crt/crt.o $LIB/cosmo-crt.o
}

for COSMOARCH in $COSMOARCHES
do
    install_yolocosmo $COSMOARCH
done

# The APE linker tooling, which the Fil-C clang driver runs as a post-link
# step for every cosmo-mode executable link:
#
#   libexec/apelink     <- rewrites the linked ELF into an APE (MZqFpD magic
#                          first), embedding the loaders and the m1 loader
#                          source; the driver runs it as
#                            apelink -V -1 -l <pizfix>/lib/ape-x86_64.elf \
#                              -o <out>.com <out>          (x86_64 links)
#                            apelink -V -1 \
#                              -l <pizfix>/lib-aarch64/ape-aarch64.elf \
#                              -M <pizfix>/libexec/ape-m1.c \
#                              -o <out>.com <out>          (aarch64 links)
#   libexec/ape-m1.c    <- the macOS-arm64 APE loader source, compiled on the
#                          fly by Xcode when an APE runs on Apple silicon
#                          (only embedded for aarch64 inputs)
#   lib/ape-x86_64.elf  <- the x86_64 APE loader binary that apelink embeds
#   lib-aarch64/ape-aarch64.elf <- the aarch64 APE loader binary
#   libexec/pecheck     <- validates the PE/Mach-O headers of an APE; handy
#                          for debugging apelink output
cp -f projects/yolocosmo/o/x86_64-optlinux/tool/build/apelink.dbg pizfix/libexec/apelink
cp -f projects/yolocosmo/o/x86_64-optlinux/tool/build/pecheck.dbg pizfix/libexec/pecheck
cp -f projects/yolocosmo/ape/ape-m1.c pizfix/libexec/ape-m1.c
cp -f projects/yolocosmo/o/x86_64/ape/ape.elf pizfix/lib/ape-x86_64.elf
cp -f projects/yolocosmo/o/aarch64/ape/ape.elf pizfix/lib-aarch64/ape-aarch64.elf
chmod +x pizfix/libexec/apelink pizfix/libexec/pecheck

# The aarch64 kernel headers (os-include's linux/asm/asm-generic per-target
# variant, which the driver points aarch64 compilations at).  Mirrors what
# build_os_include.sh does for the host architecture.
if echo "$COSMOARCHES" | grep -q aarch64
then
    rm -rf pizfix/os-include-aarch64
    mkdir -p pizfix/os-include-aarch64
    cd pizfix/os-include-aarch64
    ln -s /usr/aarch64-linux-gnu/include/linux .
    ln -s /usr/aarch64-linux-gnu/include/asm .
    ln -s /usr/aarch64-linux-gnu/include/asm-generic .
    cd ../..
fi

# Install the cosmo headers into yolo-include, so that libpas can be compiled
# with:
#
#   clang -nostdinc -isystem pizfix/yolo-include \
#       -include pizfix/yolo-include/normalize.inc
#
# The layout is:
#
#   yolo-include/*.h           <- libc/isystem/*.h (the "system" headers)
#   yolo-include/<subdir>/*.h  <- libc/isystem/<subdir>/* (sys/, net/, etc.)
#   yolo-include/libc/...      <- header-only copies of cosmo's internal
#                                 trees, since cosmo's headers cross-include
#                                 each other like "libc/foo/bar.h" and
#                                 "ape/relocations.h"
#   yolo-include/ape/...
#   yolo-include/third_party/... and yolo-include/ctl/...  <- more internal
#                                 trees that cosmo's headers reach into
#   yolo-include/normalize.inc <- and the other libc/integral/*.inc, also
#                                 copied to the top for -include convenience
rm -rf pizfix/yolo-include
mkdir -p pizfix/yolo-include

(cd projects/yolocosmo/libc/isystem && tar -cf - .) | (cd pizfix/yolo-include && tar -xf -)
(cd projects/yolocosmo && find libc ape ctl third_party -name '*.h' -o -name '*.inc' | tar -cf - -T -) | (cd pizfix/yolo-include && tar -xf -)
cp -f projects/yolocosmo/libc/integral/*.inc pizfix/yolo-include/

# ─────────────────────────────────────────────────────────────────────────────
# Cross builtins, unwinder, and libpas for the non-host architectures.
#
# The x86_64 artifacts above (and libpizlo.a etc. for the host arch, which
# build_runtime.sh builds after this script) use the host-arch compiler-rt
# and yolounwind builds.  The other architectures get their own:
#
#   libyolort.a    compiler-rt builtins (+ crtbegin/crtend), cross-compiled
#                  with the host clang (--target=<arch>-linux-gnu) in a
#                  separate compiler-rt build tree, so it never touches the
#                  host-arch compiler-rt/build
#   libyolounwind.a the tiny yolounwind, cross-compiled the same way
#   libpizlo.a     the libpas build with FILCARCH=<arch> (its Makefile keeps
#                  cross objects in libpas/build-<arch> and installs into
#                  pizfix/lib-<arch>)
#
# These run at the end so that the cross libpas build sees the cross cosmo
# marker (pizfix/lib-<arch>/libyolocosmo.a) and the installed headers.
# ─────────────────────────────────────────────────────────────────────────────
for COSMOARCH in $COSMOARCHES
do
    if test "x$COSMOARCH" = "xx86_64"
    then
        continue
    fi

    LIB=pizfix/lib-$COSMOARCH
    mkdir -p $LIB

    # compiler-rt builtins + crt for the target.  The -ffixed-x18 -ffixed-x28
    # flags keep the same aarch64 cosmo invariant as libpas/Makefile,
    # projects/usercosmo/filc.mk and build_cxx.sh (cosmo's TIB lives in x28
    # and the platform register is x18; nothing in the process may use them
    # as scratch, including crtbegin, which runs pizlonated atexit handlers
    # beneath its own frames at exit).
    if test ! -e compiler-rt/build-$COSMOARCH/lib/linux/libclang_rt.builtins-$COSMOARCH.a
    then
        mkdir -p compiler-rt/build-$COSMOARCH
        cmake -S compiler-rt -B compiler-rt/build-$COSMOARCH -G Ninja \
            -DCMAKE_BUILD_TYPE=Release \
            -DCOMPILER_RT_BUILD_BUILTINS=ON \
            -DCOMPILER_RT_BUILD_CRT=ON \
            -DCOMPILER_RT_CRT_USE_EH_FRAME_REGISTRY=ON \
            -DCOMPILER_RT_BUILD_SANITIZERS=OFF \
            -DCOMPILER_RT_BUILD_XRAY=OFF \
            -DCOMPILER_RT_BUILD_MEMPROF=OFF \
            -DCOMPILER_RT_BUILD_CTX_PROFILE=OFF \
            -DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
            -DCOMPILER_RT_BUILD_PROFILE=OFF \
            -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON \
            -DCMAKE_C_COMPILER=`which clang` \
            -DCMAKE_CXX_COMPILER=`which clang++` \
            -DCMAKE_ASM_COMPILER=`which clang` \
            -DCMAKE_C_COMPILER_TARGET=$COSMOARCH-linux-gnu \
            -DCMAKE_CXX_COMPILER_TARGET=$COSMOARCH-linux-gnu \
            -DCMAKE_ASM_COMPILER_TARGET=$COSMOARCH-linux-gnu \
            -DCMAKE_C_FLAGS="-ffixed-x18 -ffixed-x28" \
            -DCMAKE_CXX_FLAGS="-ffixed-x18 -ffixed-x28" \
            -DCMAKE_ASM_FLAGS="-ffixed-x18 -ffixed-x28" \
            -DCMAKE_C_COMPILER_WORKS=ON \
            -DCMAKE_CXX_COMPILER_WORKS=ON \
            -DCMAKE_ASM_COMPILER_WORKS=ON
        ninja -C compiler-rt/build-$COSMOARCH
    fi
    cp -f compiler-rt/build-$COSMOARCH/lib/linux/libclang_rt.builtins-$COSMOARCH.a $LIB/libyolort.a
    cp -f compiler-rt/build-$COSMOARCH/lib/linux/clang_rt.crtbegin-$COSMOARCH.o $LIB/crtbegin.o
    cp -f compiler-rt/build-$COSMOARCH/lib/linux/clang_rt.crtend-$COSMOARCH.o $LIB/crtend.o

    # The tiny unwinder for the target.
    (cd yolounwind && make clean FILCARCH=$COSMOARCH && make FILCARCH=$COSMOARCH)
    cp -f yolounwind/libyolounwind-$COSMOARCH.a $LIB/libyolounwind.a

    # libpas (libpizlo.a + filc_crt.o + filc_mincrt.o) for the target.  This
    # is compile-and-archive only, so it works even before the pizlonated
    # user libc exists (which compute-closure.py needs - the bootstrapping
    # order is: full cosmo build, this script's installs, build_usercosmo.sh,
    # then a rerun of compute-closure.py if the closure changed).
    # Makefile-setup installs the stdfil headers into pizfix, which the
    # filc/src objects need; the host build in build_runtime.sh runs it
    # later, but this script runs before that on a fresh tree.
    mkdir -p pizfix/stdfil-include
    make -C libpas -f Makefile-setup FILC_OUTPUT_ROOT=`pwd`/pizfix
    make -C libpas -f Makefile FILCARCH=$COSMOARCH \
        FILC_OUTPUT_ROOT=`pwd`/pizfix \
        FILC_CLANG=`pwd`/build/bin/clang \
        all
done
