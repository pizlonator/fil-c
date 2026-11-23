#!/bin/sh
#
# Copyright (c) 2024-2025 Epic Games, Inc. All Rights Reserved.
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

cd projects/Python-3.12.5
extract_source
# py_cv_module__uuid=n/a: pizfix has never shipped Python's _uuid module, and
# we keep it that way. The util-linux build (build_glibc_extras.sh) installs a
# libuuid pkg-config file into pizfix, and Python 3.12's configure enables
# _uuid whenever that .pc exists -- but the .pc's includedir points through the
# since-deleted projects/util-linux/extracted-source build tree, so the _uuid
# compile cannot find <uuid.h>. Disable the module explicitly. Note that
# Python 3.12.5's configure only honors the n/a override here: a "disabled"
# cache value is silently recomputed (and re-enabled) for default-enabled
# modules, while n/a skips the recompute so the module never enters
# Modules/Setup.stdlib.
PATH=$PWD/../../../pizfix/bin:$PATH CC="$CCPREFIX$PWD/../../../build/bin/clang -g -O2" ./configure \
    --without-mimalloc --without-pymalloc --without-freelists \
    --disable-test-modules py_cv_module__uuid=n/a \
    --prefix=$PWD/../../../pizfix
$MAKE -j $NCPU
$MAKE -j $NCPU install

