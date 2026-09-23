#!/bin/sh
#
# Copyright (c) 2023-2025 Epic Games, Inc. All Rights Reserved.
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

. libpas/common.sh

set -e
set -x

NINJAFLAGS=
NINJARUNTIMEFLAGS=
if test -e clang-build-overrides.sh
then
    . ./clang-build-overrides.sh
fi

# The C++ runtimes (libc++ and libc++abi) are built as a standalone cmake
# project against the Fil-C compiler, separate from the LLVM build that
# configure_llvm.sh configures. This is done instead of using the LLVM build's
# "runtimes" target, so that changing runtimes-only options does not require
# rebuilding LLVM and so that building the runtimes does not drag in unrelated
# LLVM targets. We nuke and reconfigure runtimes-build every time this script
# runs, because build_cxx.sh is invoked downstream from a rebuild of the user
# libc. The musl-or-not option (ALTLLVMLIBCOPT, exported by setup_glibc.sh for
# glibc builds) only affects this cmake invocation, so changing it does not
# require rebuilding LLVM.

# The cosmo flavor is detected by probing for the yolo cosmo libc archive, the
# same marker that the driver and filc/run-tests use.  Cosmo's user libc is
# musl-like, so libc++ uses the same pthread/thread surface, but it gets its
# own _LIBCPP_HAS_COSMO_LIBC knob (see libcxx/CMakeLists.txt) and it must be
# built static-only, since cosmo mode has no shared libraries (the driver
# refuses -shared and links -static).
if test -e pizfix/lib/libyolocosmo.a
then
    IS_COSMO=1
else
    IS_COSMO=0
fi

if test "x$ALTLLVMLIBCOPT" = "x"
then
    LLVMLIBCOPT="-DLIBCXX_HAS_MUSL_LIBC=ON"
else
    LLVMLIBCOPT=$ALTLLVMLIBCOPT
fi

SHARED_LIBS=

if test "x$IS_COSMO" = "x1"
then
    # Explicitly pin the musl knob off so that a stale cache entry cannot leak
    # into the cosmo configuration, and build static archives only.  The musl
    # and glibc flavors keep getting both .a and .so, exactly as before.
    LLVMLIBCOPT="-DLIBCXX_HAS_MUSL_LIBC=OFF -DLIBCXX_HAS_COSMO_LIBC=ON"
    SHARED_LIBS="-DLIBCXX_ENABLE_SHARED=OFF -DLIBCXXABI_ENABLE_SHARED=OFF"
fi

test -e build/bin/clang -a -e build/bin/clang++

TRIPLE=`$PWD/build/bin/clang -print-target-triple`

rm -rf runtimes-build build/runtimes
mkdir -p runtimes-build

cmake -S runtimes -B runtimes-build -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_COMPILER=$PWD/build/bin/clang \
    -DCMAKE_CXX_COMPILER=$PWD/build/bin/clang++ \
    -DCMAKE_ASM_COMPILER=$PWD/build/bin/clang \
    -DCMAKE_C_COMPILER_WORKS=ON \
    -DCMAKE_CXX_COMPILER_WORKS=ON \
    -DCMAKE_ASM_COMPILER_WORKS=ON \
    -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi" \
    -DLLVM_DEFAULT_TARGET_TRIPLE=$TRIPLE \
    -DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=ON \
    -DLIBCXXABI_HAS_PTHREAD_API=ON -DLIBCXX_ENABLE_EXCEPTIONS=ON \
    -DLIBCXXABI_ENABLE_EXCEPTIONS=ON -DLIBCXX_HAS_PTHREAD_API=ON \
    $LLVMLIBCOPT -DLIBCXXABI_USE_LLVM_UNWINDER=OFF \
    $SHARED_LIBS \
    -DLIBCXX_FORCE_LIBCXXABI=ON \
    -DLLVM_ENABLE_ASSERTIONS=ON \
    -DLIBCXX_HARDENING_MODE=extensive \
    -DLLVM_INCLUDE_TESTS=OFF

(cd runtimes-build && ninja $NINJAFLAGS $NINJARUNTIMEFLAGS)

./install-cxx-$OS.sh
# __config_site is architecture-independent for matching libc/libc++ builds.
rm -rf build/include/$CROSSARCH-unknown-linux-gnu
cp -R build/include/$ARCH-unknown-linux-gnu build/include/$CROSSARCH-unknown-linux-gnu
./fix_clang.sh


