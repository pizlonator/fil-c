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

# Builds OpenSSL 3.6.4 (vendored at projects/openssl-3.6.4, so unlike
# build_openssl.sh there is no extract_source step and the relative paths go
# up two levels, not three) with assembly enabled: the perlasm generators
# emit sarcasm annotations (gated on SARCASM=1) and Fil-C clang assembles the
# generated .s/.S files with sarcasm (no -yolo-assembler!).
#
# no-padlockeng: the padlock engine's rep-movsq/xcrypt implicit-memory
# instructions are unmodelable in sarcasm (documented decision).
#
# This deliberately does NOT run make install: OpenSSL 3.5.7 (built by
# build_openssl.sh) still owns pizfix's libcrypto.so.3/libssl.so.3.
cd projects/openssl-3.6.4

# If a previous configure (e.g. a no-asm one) left configdata.pm behind,
# distclean it away so that everything, including the perlasm-generated
# assembly, is rebuilt with the configuration below.
if test -f configdata.pm
then
    make distclean || true
    rm -f configdata.pm
fi

# The perlasm .pl files gate their sarcasm hunks (page-walk removals, frame
# restructures, avx2 delegations) on this; x86_64-xlate.pl's signature/global
# annotations are always emitted as gas-compatible `#!' comments.
export SARCASM=1

CC="$PWD/../../build/bin/clang -g -O2" ./Configure \
    zlib no-padlockeng --prefix=$PWD/../../pizfix --libdir=lib
make -j $NCPU

# Only run the test suite in a glibc build: PIZFIX_LIB/libc.so.6666 is the glibc
# user-lib marker installed by the glibc build (musl builds lack it), and the test
# suite only passes on glibc (see sarcastic-openssl.txt).
#
# Retry the suite once if it fails: timing-sensitive QUIC tests are flaky
# under full parallelism (e.g. 70-test_quic_multistream.t failed with
# "shutdown time exceeded 5sec" at test/quic_multistream_test.c:1257 in
# script 88 op 17; 70-test_quic_radix.t has a similar wall-clock flake in
# test/radix/terp.c). They pass reliably on a re-run.
PIZFIX_LIB=../../pizfix/lib
if test -e "$PIZFIX_LIB/libc.so.6666"
then
    HARNESS_JOBS=${HARNESS_JOBS:-$NCPU} make test || HARNESS_JOBS=${HARNESS_JOBS:-$NCPU} make test
fi
