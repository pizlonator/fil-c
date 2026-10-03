#!/bin/sh
#
# Copyright (c) 2024 Epic Games, Inc. All Rights Reserved.
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
# THIS SOFTWARE IS PROVIDED BY FILIP PIZLO. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

set -e
set -x

arch=$(uname -m)

# Install the C++ headers into the LLVM build tree, where the Fil-C compiler
# driver and the optfil/pizlix installers expect to find them.
rm -rf build/include/c++
cp -R runtimes-build/include/c++ build/include/c++
mkdir -p build/include/$arch-unknown-linux-gnu
rm -rf build/include/$arch-unknown-linux-gnu/c++
cp -R runtimes-build/include/$arch-unknown-linux-gnu/c++ build/include/$arch-unknown-linux-gnu/c++

# Also mirror the C++ modules into the LLVM build tree.
mkdir -p build/modules
rm -rf build/modules/c++
cp -R runtimes-build/modules/c++ build/modules/c++

# Cosmo mode (see build_cxx.sh for the flavor probe) is static-only: cosmo
# executables cannot use shared libraries, so only the .a archives get
# installed into pizfix.
IS_COSMO=0
if test -e pizfix/lib/libyolocosmo.a
then
    IS_COSMO=1
fi

if test "x$IS_COSMO" = "x1"
then
    cp runtimes-build/lib/$arch-unknown-linux-gnu/libc++.a pizfix/lib
    cp runtimes-build/lib/$arch-unknown-linux-gnu/libc++abi.a pizfix/lib
    cp runtimes-build/lib/$arch-unknown-linux-gnu/libc++experimental.a pizfix/lib
else
    cp runtimes-build/lib/$arch-unknown-linux-gnu/libc++.so pizfix/lib
    cp runtimes-build/lib/$arch-unknown-linux-gnu/libc++.so.1.0 pizfix/lib
    cp runtimes-build/lib/$arch-unknown-linux-gnu/libc++abi.so.1.0 pizfix/lib
    cp runtimes-build/lib/$arch-unknown-linux-gnu/libc++.a pizfix/lib
    cp runtimes-build/lib/$arch-unknown-linux-gnu/libc++abi.a pizfix/lib
    cp runtimes-build/lib/$arch-unknown-linux-gnu/libc++experimental.a pizfix/lib
    (cd pizfix/lib &&
         rm -f libc++.so.1 &&
         rm -f libc++abi.so.1 &&
         rm -f libc++abi.so &&
         ln -s libc++.so.1.0 libc++.so.1 &&
         ln -s libc++abi.so.1.0 libc++abi.so.1 &&
         ln -s libc++abi.so.1 libc++abi.so)
fi
