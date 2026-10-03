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

# The driver resolves the C++ stdlib headers from build/include/c++ (see
# install-cxx-*.sh, which installs them there after the build).  If a stale
# copy from a previous run is present while the runtimes are being compiled,
# libc++'s C-compatibility headers (string.h, errno.h, ...) include_next into
# that stale copy, whose include guards collide with the copies being compiled
# from runtimes-build/include, and the underlying libc header is never reached
# (e.g. `strcmp` ends up undeclared).  So make sure the driver's copy doesn't
# exist while ninja runs; install-cxx-*.sh recreates it below.
rm -rf build/include/c++ build/include/$TRIPLE/c++

(cd runtimes-build && ninja $NINJAFLAGS $NINJARUNTIMEFLAGS)

# The aarch64 flavor of the C++ runtime, when the aarch64 cosmo tree exists
# (build_yolocosmo.sh installs pizfix/lib-aarch64/libyolocosmo.a).  Mirrors
# the host build above, with --target=aarch64-linux-gnu and a separate build
# tree so nothing collides; cosmo mode is static-only here too.
#
# The -ffixed-x18 -ffixed-x28 flags match the pizlonated libc
# (projects/usercosmo/filc.mk) and libpas (libpas/Makefile) aarch64 cosmo
# builds: cosmo keeps its TIB in x28 on aarch64 and every yolo-side code
# path that pizlonated code can enter (libpizlo's pthread_getspecific-based
# filc_get_my_thread() among them) reads it from x28, so no pizlonated code
# may use x28 (or the platform register x18) as scratch.  Without it, LLVM
# happily allocates x28 inside libc++/libc++abi functions and the first
# iostream-style global ctor that allocates trips filc_pollcheck_slow's
# my_thread == filc_get_my_thread() assertion.  The Fil-C driver injects the
# same flags for user compiles (see Linux::addClangTargetOptions).
if test -e pizfix/lib-aarch64/libyolocosmo.a
then
    rm -rf runtimes-build-aarch64
    mkdir -p runtimes-build-aarch64

    cmake -S runtimes -B runtimes-build-aarch64 -G Ninja \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_C_COMPILER=$PWD/build/bin/clang \
        -DCMAKE_CXX_COMPILER=$PWD/build/bin/clang++ \
        -DCMAKE_ASM_COMPILER=$PWD/build/bin/clang \
        -DCMAKE_C_COMPILER_WORKS=ON \
        -DCMAKE_CXX_COMPILER_WORKS=ON \
        -DCMAKE_ASM_COMPILER_WORKS=ON \
        -DCMAKE_C_FLAGS="--target=aarch64-linux-gnu -ffixed-x18 -ffixed-x28" \
        -DCMAKE_CXX_FLAGS="--target=aarch64-linux-gnu -ffixed-x18 -ffixed-x28" \
        -DCMAKE_ASM_FLAGS="--target=aarch64-linux-gnu -ffixed-x18 -ffixed-x28" \
        -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi" \
        -DLLVM_DEFAULT_TARGET_TRIPLE=aarch64-unknown-linux-gnu \
        -DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=ON \
        -DLIBCXXABI_HAS_PTHREAD_API=ON -DLIBCXX_ENABLE_EXCEPTIONS=ON \
        -DLIBCXXABI_ENABLE_EXCEPTIONS=ON -DLIBCXX_HAS_PTHREAD_API=ON \
        -DLIBCXX_HAS_MUSL_LIBC=OFF -DLIBCXX_HAS_COSMO_LIBC=ON \
        -DLIBCXXABI_USE_LLVM_UNWINDER=OFF \
        -DLIBCXX_ENABLE_SHARED=OFF -DLIBCXXABI_ENABLE_SHARED=OFF \
        -DLIBCXX_FORCE_LIBCXXABI=ON \
        -DLLVM_ENABLE_ASSERTIONS=ON \
        -DLIBCXX_HARDENING_MODE=extensive \
        -DLLVM_INCLUDE_TESTS=OFF

    # Same stale-header dance as above: install-cxx-$OS.sh recreated
    # build/include/c++ for the host, which would shadow the headers this
    # aarch64 build compiles from runtimes-build-aarch64/include.
    rm -rf build/include/c++ build/include/aarch64-unknown-linux-gnu/c++

    (cd runtimes-build-aarch64 && ninja $NINJAFLAGS $NINJARUNTIMEFLAGS)

    mkdir -p pizfix/lib-aarch64
    cp runtimes-build-aarch64/lib/aarch64-unknown-linux-gnu/libc++.a pizfix/lib-aarch64
    cp runtimes-build-aarch64/lib/aarch64-unknown-linux-gnu/libc++abi.a pizfix/lib-aarch64
    cp runtimes-build-aarch64/lib/aarch64-unknown-linux-gnu/libc++experimental.a pizfix/lib-aarch64
    rm -rf build/include/aarch64-unknown-linux-gnu/c++
    mkdir -p build/include/aarch64-unknown-linux-gnu
    cp -R runtimes-build-aarch64/include/aarch64-unknown-linux-gnu/c++ \
        build/include/aarch64-unknown-linux-gnu/c++
fi

# Install the host C++ headers and archives into the LLVM build tree and
# pizfix.  This has to happen after the aarch64 section above (if it ran),
# because the aarch64 ninja needs the host header copies out of the way for
# the same include_next-shadowing reason as the host ninja did.
./install-cxx-$OS.sh

./fix_clang.sh


