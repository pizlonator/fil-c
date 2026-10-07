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

. libpas/common.sh

set -e

set -x

# minilute builds in-place (it is not an extracted source archive). Its
# Makefile builds the vendored luau (projects/lute-1.0.0/extern/luau, release
# mode) and links the static libs; CC/CXX select the toolchain.
cd projects/minilute

# build_base.sh wipes pizfix, so any previously built minilute/luau objects
# were compiled and linked against the wiped libc and are stale (they may
# reference sonames/symbols the fresh libc no longer provides). Always force
# a from-scratch build of minilute and the vendored luau; it only takes
# seconds and keeps this script correct when run standalone, too.
make distclean

# minilute is a build tool: a plain host x86_64 ELF is what we want, not an
# APE.  --filc-no-ape disables the Fil-C driver's default cosmo behavior
# (aarch64 shadow compiles and the fat-APE post-link), which would double this
# build and leave fat-APE side files (minilute.com.dbg, minilute.aarch64.elf)
# behind - the cp below installs the result into pizfix/bin.
make -j $NCPU CC="$PWD/../../build/bin/clang --filc-no-ape" CXX="$PWD/../../build/bin/clang++ --filc-no-ape"

cp minilute ../../pizfix/bin/

# Validation (run from the repo root): the smoke test exercises relative
# requires, @lute/fs, and @lute/process; the sarcasm invocation proves the
# real assembler frontend works.
cd ../..

pizfix/bin/minilute projects/minilute/tests/smoke.luau

pizfix/bin/minilute projects/sarcasm/sarcasm-cli.luau --version
