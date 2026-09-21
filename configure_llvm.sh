#!/bin/sh
#
# Copyright (c) 2023-2025 Epic Games, Inc. All Rights Reserved.
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

. libpas/common.sh

set -e
set -x

# Note: the C++ runtimes (libc++ and libc++abi) are not configured here. They
# are built as a standalone cmake project against the Fil-C compiler by
# build_cxx.sh, so that changing runtimes-only options (like whether the
# runtimes are built against musl or glibc) never forces LLVM to be rebuilt.

# Build both backends so this compiler can target either supported architecture.
export CMAKEOPTIONS="-S ../llvm -B . -G Ninja -DLLVM_ENABLE_PROJECTS=clang
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLLVM_ENABLE_ASSERTIONS=ON
    -DLLVM_ENABLE_LLD=ON
    -DLLVM_TARGETS_TO_BUILD=X86;AArch64
    -DLLVM_ENABLE_LIBXML2=OFF -DLLVM_ENABLE_LIBEDIT=OFF
    -DLLVM_ENABLE_LIBPFM=OFF -DLLVM_ENABLE_ZLIB=OFF -DLLVM_ENABLE_ZSTD=OFF
    -DLLVM_ENABLE_CURL=OFF -DLLVM_ENABLE_HTTPLIB=OFF
    -DLLVM_STATIC_LINK_CXX_STDLIB=ON -DCMAKE_EXE_LINKER_FLAGS=-static-libgcc"

./configure_cmake_project.sh

