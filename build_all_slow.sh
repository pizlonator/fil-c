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

set -e
set -x

test ! -d projects/pizfix

./build_ffi.sh
./build_pkgconf.sh
./build_dash.sh
./build_wg14_signals.sh
./build_libuev.sh
./build_icu.sh
./build_zlib.sh
./build_bzip2.sh
./build_bzip3.sh
./build_xz.sh
./build_zstd.sh
./build_expat.sh
./build_pcre.sh      # Hilariously, pcre + pcre2 would like to depend on libedit, but libedit depends
./build_pcre2.sh     # on ncurses, and ncurses depends on pcre2. Luckily, only pcretest wants libedit.
./build_jpeg-6b.sh
./build_ncurses.sh
./build_libedit.sh
./build_openssl.sh
./build_nghttp2.sh
./build_curl.sh
./build_openssh.sh
./build_mg.sh
./build_tcl.sh
./build_sqlite.sh
./build_cpython.sh
./build_zsh.sh
./build_lua.sh
./build_simdutf.sh
./build_quickjs.sh

# Disable these because:
# - We only need them for test coverage.
# - They haven't ever caught a regression.
# - To run the tests these builds have to download things from the intertubes, and I've seen the
#   downloads fail flakily, causing the whole build to fail.
# - They take forever to build.
#./build_simdjson.sh
#./build_ada.sh

./build_toybox.sh
./build_libevent.sh
./build_tmux.sh
./build_libuv.sh
./build_benchmarks.sh

test ! -d projects/pizfix
