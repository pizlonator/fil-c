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

cd projects
rm -rf blake3/extracted-source
../filc/projeny extract blake3.projeny blake3/extracted-source
cd blake3/extracted-source
BLAKE3_SIMD_TYPE=x86-intrinsics
if [ "$ARCH" = aarch64 ]; then
    BLAKE3_SIMD_TYPE=neon-intrinsics
fi
CC=$PWD/../../../build/bin/clang CXX=$PWD/../../../build/bin/clang++ cmake -S c -B c/build -DCMAKE_INSTALL_PREFIX=$PWD/../../../pizfix -DBLAKE3_SIMD_TYPE=$BLAKE3_SIMD_TYPE -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build c/build --target install -j $NCPU
../../../build/bin/clang -o example c/example.c -lblake3 -O2 -g
test `./example < README.md` = "197178ed9049471915163b3a24bb06eef4d8171cb9a5ed2f26040aacb23acb13"
test `./example < ../../../benchmarkData/Pizigani_1367_Chart_10MB.pnm` = "5eefbdaa7deb1c614f17d39f7f1d6274856f005a7d3e3543f17c5bac4fbb9d25"
cd ..
rm -rf extracted-source

# I would need to build the onetbb thing to do this.
# ../../../build/bin/clang -o example_tbb c/example_tbb.c -lblake3 -O2 -g
# test `./example_tbb ../../../benchmarkData/Pizigani_1367_Chart_10MB.pnm` = "a5fdca3e301ce0f1b4bf92e9532fdd731842715b244b26f393404796a1c15b06"

