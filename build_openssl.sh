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
# THIS SOFTWARE IS PROVIDED BY FILIP PZILO ``AS IS AND ANY
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
# Extract the projeny source OUTSIDE the projeny workdir (projects/openssl):
# the configured build tree has to stay behind after this script finishes
# (build_perl.sh regenerates der_digests.h from it with pizfix perl), and
# keeping the projeny workdir pristine keeps `projeny rebase` usable.
rm -rf openssl-build/extracted-source
mkdir -p openssl-build
../filc/projeny extract openssl.projeny openssl-build/extracted-source
cd openssl-build/extracted-source
OPENSSL_ARGS="zlib"
CC="$PWD/../../../build/bin/clang -g -O2 -yolo-assembler" ./Configure \
    $OPENSSL_ARGS --prefix=$PWD/../../../pizfix --libdir=lib
make -j $NCPU

# Only run the test suite in a glibc build. There are a bunch of failures in the test suite in a musl
# build.
#
# Retry the suite once if it fails: 70-test_quic_radix.t (check_pc_flood) has a flaky
# wall-clock timeout ("timed out while executing op 33" in test/radix/terp.c) when the
# whole suite runs with full parallelism; it passes reliably on a re-run.
if test -e ../../../pizfix/lib/libc.so.6666
then
    HARNESS_JOBS=$NCPU make test || HARNESS_JOBS=$NCPU make test
fi

make -j $NCPU install_sw
make -j $NCPU install_ssldirs
