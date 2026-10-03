# projeny - project tarball+patch manager.
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
# THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
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
# Runtime helpers shelled out to (direct posix_spawnp, no shell): tar, cp.
# Diff, patch application, and three-way merge are implemented internally
# (git-compatible unified diffs); projeny never invokes git.
# URL:-based .projeny files download their tarball with libcurl (linked in,
# curl_easy API) and verify it with blake3 (vendored portable C in
# src/blake3/, SIMD disabled, CC0 — compiled in, blake3_hasher API), so
# libcurl is the only external build requirement.
# Build requires only a C++17 compiler (plus a C compiler for the vendored
# blake3).
CC ?= cc
CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -g -Wall -Wextra
CFLAGS ?= -O2 -g -Wall -Wextra
CPPFLAGS ?=
LDFLAGS ?=
# std::thread (util's run_parallel, the parallel hash checks) needs -pthread
# on GNU/Linux. Kept as its own overridable variable — appended in the rules,
# so an externally-provided CXXFLAGS still gets it, and a platform where the
# flag is wrong can set PTHREAD_FLAGS= empty.
PTHREAD_FLAGS ?= -pthread
# The vendored blake3 (upstream 1.8.7 portable C, CC0 — see
# src/blake3/README): compile its three sources with BLAKE3's documented
# portable flags, which disable all SIMD paths so no intrinsics or assembly
# are needed and the code builds warning-free with any C compiler.
BLAKE3_SRC := src/blake3/blake3.c src/blake3/blake3_dispatch.c src/blake3/blake3_portable.c
BLAKE3_FLAGS := -DBLAKE3_NO_SSE2 -DBLAKE3_NO_SSE41 -DBLAKE3_NO_AVX2 -DBLAKE3_NO_AVX512 -DBLAKE3_USE_NEON=0
LDLIBS ?= -lcurl

# Abort a target the moment one of its recipe commands fails, so a failing
# compile never leaves a truncated .o (or binary) behind to be linked later.
.DELETE_ON_ERROR:

# Set BUILD_DIR to build out-of-tree: all build products (object files, dep
# files, and the projeny binary) land in $(BUILD_DIR) instead of next to the
# sources. Each BUILD_DIR is independent, so multiple compilers (e.g. the
# system compiler and Fil-C) can share this source tree without clobbering
# each other's build products. The default builds in-tree, as before.
BUILD_DIR ?= .

SRC := src/main.cc src/util.cc src/projeny_file.cc src/tree.cc src/ops.cc src/vcs.cc src/download.cc
# The vendored blake3 objects join the C++ ones in OBJ (DEP derives from
# OBJ, so -MMD -MP header dependencies keep working for both).
OBJ := $(addprefix $(BUILD_DIR)/,$(SRC:.cc=.o) $(BLAKE3_SRC:.c=.o))
DEP := $(OBJ:.o=.d)
BIN := $(BUILD_DIR)/projeny

all: $(BIN)

$(BIN): $(OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(PTHREAD_FLAGS) -o $@ $(OBJ) $(LDFLAGS) $(LDLIBS)

# Automatic header dependencies; parallel-safe (-j). The .cc rule builds
# the projeny sources; the .c rule builds the vendored blake3 with its
# SIMD-disabling flags.
$(BUILD_DIR)/%.o: %.cc
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(PTHREAD_FLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(BLAKE3_FLAGS) -MMD -MP -c $< -o $@

-include $(DEP)

test check: all
	./tests/run_tests.sh $(BIN)

clean:
	rm -f $(BIN) $(OBJ) $(DEP)
	-rmdir $(BUILD_DIR)/src/blake3 $(BUILD_DIR)/src $(BUILD_DIR) 2>/dev/null

.PHONY: all test check clean
