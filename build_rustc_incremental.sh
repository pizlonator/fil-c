#!/bin/bash
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
#
# Build Fil-C's rustc: incremental build + test.
#
# This reuses an existing rust-build/ -- extraction, the LLVM swap, the
# Fil-C version stamp and the compiler build only redo work when what they
# produce is missing or changed, so a re-run is usually a fast no-op (a
# missing rust-build/ is bootstrapped from scratch, so this script doubles
# as a first build) -- and then runs the Fil-Rust test suite
# (filrustc/run-tests).  For a clean build use build_rustc.sh, which nukes
# rust-build/ first.  All of the logic lives in build_rustc_impl.sh (shared
# with build_rustc.sh).
#
# Usage: ./build_rustc_incremental.sh   (incremental build + test)
#        ./build_rustc.sh               (clean build + test)
#        export SKIP_RUST_TESTS=1 to skip the test suite.
#
# The result is rust-build/rustc (symlink, also as rust-build/filrustc).
# NOTE: the built rustc resolves its `filcc` linker itself, so nothing extra
# has to be on PATH: it tries the directory baked in at build time
# ($FILC_LINKER_DIR, captured from `command -v filcc` when the compiler was
# built), then walks up from the rustc binary for <ancestor>/build/bin/filcc,
# then /opt/fil/bin/filcc, and only then falls through to the PATH.  A baked
# or discovered filcc wins over the PATH by design (an explicit
# `-C linker=/path/to/filcc` overrides it).  Putting $FILC_ROOT/build/bin on
# PATH when using the built rustc is still the supported flow (and is how
# this script itself finds filcc to bake in), but it is no longer required.
set -e
set -o pipefail
set -x

. "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/build_rustc_impl.sh"

do_build_rustc incremental
