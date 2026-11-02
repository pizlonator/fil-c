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
# Build Fil-C's rustc: clean build + test by default.
#
# This nukes rust-build/ and rebuilds the compiler from scratch, then runs
# the Fil-Rust test suite (filrustc/run-tests).  All of the logic lives in
# build_rustc_impl.sh (shared with build_rustc_incremental.sh, which reuses
# rust-build/ instead of nuking it): the impl extracts the patched Rust
# 1.90.0 sources from the projeny checkout in projects/rust into
# rust-build/rust, swaps Rust's bundled LLVM for the Fil-C LLVM (both are
# LLVM 20.1.8, so rustc's LLVM C++ wrapper compiles against it unchanged
# modulo our Fil-C pipeline patches), writes a bootstrap config.toml, and
# then runs `x.py build --stage 1`.  The resulting stage 1 rustc pizlonates
# every module it compiles (the Fil-Pizlonator runs in every pipeline, at
# every optimization level) and the x86_64-unknown-linux-gnu target spec
# makes `filcc` the linker, so what it produces links as Fil-C.
#
# The built rustc also self-identifies as Fil-C, clang-style: `rustc
# --version` appends " (Fil-C <version> <repo-url> <hash>)"; see
# build_rustc_impl.sh.
#
# Usage: ./build_rustc.sh               (clean build + test)
#        ./build_rustc_incremental.sh   (incremental build + test)
#        export SKIP_RUST_TESTS=1 to skip the test suite.
#
# The result is rust-build/rustc (symlink, also as rust-build/filrustc).
# NOTE: $FILC_ROOT/build/bin must be on PATH whenever the built rustc is
# used, because linking goes through Fil-C's `filcc`.
set -e
set -o pipefail
set -x

. "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/build_rustc_impl.sh"

do_build_rustc clean
