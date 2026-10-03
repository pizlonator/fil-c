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

# Builds "usercosmo": cosmopolitan libc compiled BY the Fil-C compiler.  This
# mirrors build_usermusl.sh (the musl flavor) for the cosmo flavor.  It
# produces:
#
#   pizfix/lib/libc.a   the pizlonated cosmo libc (static)
#   pizfix/lib/libm.a   an empty archive so that -lm resolves
#   pizfix/lib/crt1.o   the process entry object (yolo cosmo crt + yolo glue)
#   pizfix/include/     cosmo public headers (the flavor switch for the driver)
#
# All of the build logic lives in projects/usercosmo/filc.mk; this wrapper
# checks the environment, drives make, and prints the summary.  It builds
# every architecture in COSMOARCHES (default "x86_64 aarch64"): the host
# architecture installs into pizfix/lib and pizfix/include (the headers are
# arch-neutral, so only the host build installs them), and the others cross
# build into pizfix/lib-<arch>.  Force a full rebuild of the libc with
# `FORCE=1 ./build_usercosmo.sh`.

. libpas/common.sh

set -e
set -x

ROOT=$PWD

COSMOARCHES=${COSMOARCHES:-"x86_64 aarch64"}

for COSMOARCH in $COSMOARCHES
do
    if test "x$COSMOARCH" = "x$(uname -m)"
    then
        BUILDDIR=o-filc
    else
        BUILDDIR=o-filc-$COSMOARCH
    fi
    make -C "$ROOT/projects/usercosmo" -f filc.mk -j "$NCPU" install \
        PIZFIX="$ROOT/pizfix" \
        FILC_CLANG="$ROOT/build/bin/clang" \
        FILCARCH="$COSMOARCH" \
        FILC_BUILDDIR="$BUILDDIR"
done

echo ""
echo "usercosmo build complete:"
for COSMOARCH in $COSMOARCHES
do
    if test "x$COSMOARCH" = "xx86_64"
    then
        LIB="$ROOT/pizfix/lib"
    else
        LIB="$ROOT/pizfix/lib-$COSMOARCH"
    fi
    echo "  [$COSMOARCH] libc.a = $(ls -la "$LIB/libc.a" | awk '{print $5}') bytes, $(ar t "$LIB/libc.a" | wc -l) members"
    echo "  [$COSMOARCH] crt1.o = yolo cosmo crt + yolo glue"
done
echo "  headers = $ROOT/pizfix/include (cosmo flavor, arch-neutral)"
echo ""
echo "Try: build/bin/clang -o /tmp/cosmohello /tmp/hello.c && /tmp/cosmohello"
echo "     build/bin/clang --target=aarch64-linux-gnu -o /tmp/cosmohello-arm /tmp/hello.c"
