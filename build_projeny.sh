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

# Fil-C build of projeny: compile projects/projeny with Fil-C++ (build/bin),
# run the projeny test suite (fails the build on test failure). This builds
# into its own build directory (BUILD_DIR=build-filc) so that it does not
# dirty the yolo build left behind by build_projeny_yolo.sh, which uses a
# different build directory.

cd projects/projeny

$MAKE clean BUILD_DIR=build-filc

$MAKE -j $NCPU BUILD_DIR=build-filc CC="$PWD/../../build/bin/clang" CXX="$PWD/../../build/bin/clang++"

$MAKE test BUILD_DIR=build-filc CC="$PWD/../../build/bin/clang" CXX="$PWD/../../build/bin/clang++"

