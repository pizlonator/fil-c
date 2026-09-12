#!/bin/sh
#
# Copyright (c) 2024-2025 Epic Games, Inc. All Rights Reserved.
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

# Depth-robust paths: resolve everything from this script's own directory so
# the script works no matter where it is invoked from (no relative cd chains
# like `cd projects` that depend on the caller's CWD).
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECTS_DIR="$SCRIPT_DIR/projects"
WORKDIR="$PROJECTS_DIR/icu/extracted-source"
PROJENY_BIN="$SCRIPT_DIR/filc/projeny"
PROJENY_FILE="$PROJECTS_DIR/icu.projeny"
CC_BIN="$SCRIPT_DIR/build/bin/clang"
CXX_BIN="$SCRIPT_DIR/build/bin/clang++"
PREFIX="$SCRIPT_DIR/pizfix"

# Always remove the scratch extraction tree on exit/interrupt, even if the
# build below fails. The ${WORKDIR:?} expansion aborts instead of running
# rm -rf on an empty path if WORKDIR is ever unset (safe rm).
cleanup() {
    rm -rf "${WORKDIR:?}"
}
trap cleanup EXIT INT TERM

rm -rf "${WORKDIR:?}"
"$PROJENY_BIN" extract "$PROJENY_FILE" "$WORKDIR"
cd "$WORKDIR/source"
THE_OS=Linux THE_COMP="the Clang C++" CC="$CC_BIN" CXX="$CXX_BIN" CFLAGS="-O3 -g" CXXFLAGS="-O3 -g" ./configure --enable-debug --prefix="$PREFIX"
make -j $NCPU

make -j $NCPU check

make -j $NCPU install
