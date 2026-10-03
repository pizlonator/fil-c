#!/bin/bash
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

set -e
set -x

ulimit -c unlimited

test `id -u` -eq 0

test "x$FILCSRC" != "x"
test -d $FILCSRC/filc
test -d $FILCSRC/projects
test -d $FILCSRC/optfil
test -d /opt/fil/bin

test -e $FILCSRC/build/bin/clang-20

cd /opt/fil
cp -v $FILCSRC/build/bin/clang-20 bin/filcc-clang-20
strip bin/filcc-clang-20
patchelf --remove-rpath bin/filcc-clang-20

ARCH=`uname -m`
case $ARCH in
    x86_64)
        OLDLDNAME=ld-linux-${ARCH//_/-}.so.2
        ;;
    aarch64)
        # On aarch64, glibc's dynamic loader is ld-linux-aarch64.so.1 (the
        # .so.1 suffix is correct for aarch64; x86_64 uses .so.2).
        OLDLDNAME=ld-linux-aarch64.so.1
        ;;
    *)
        echo "Unsupported arch: $ARCH"
        exit 1
        ;;
esac
patchelf --set-interpreter /opt/fil/lib/ld-fil1-$ARCH.so bin/filcc-clang-20
patchelf --replace-needed $OLDLDNAME ld-fil1-$ARCH.so bin/filcc-clang-20
patchelf --replace-needed libc.so.6 libyolocimpl.so bin/filcc-clang-20
patchelf --replace-needed libm.so.6 libyolomimpl.so bin/filcc-clang-20

