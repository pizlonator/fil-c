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
# Build requires only a C++17 compiler.
CXX ?= g++
CC ?= cc
CXXFLAGS ?= -std=c++17 -O2 -g -Wall -Wextra
CPPFLAGS ?=
LDFLAGS ?=

# Set BUILD_DIR to build out-of-tree: all build products (object files, dep
# files, and the projeny binary) land in $(BUILD_DIR) instead of next to the
# sources. Each BUILD_DIR is independent, so multiple compilers (e.g. the
# system compiler and Fil-C) can share this source tree without clobbering
# each other's build products. The default builds in-tree, as before.
BUILD_DIR ?= .

SRC := src/main.cc src/util.cc src/projeny_file.cc src/tree.cc src/ops.cc src/vcs.cc
OBJ := $(addprefix $(BUILD_DIR)/,$(SRC:.cc=.o))
DEP := $(OBJ:.o=.d)
BIN := $(BUILD_DIR)/projeny

all: $(BIN)

$(BIN): $(OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ) $(LDFLAGS)

# Automatic header dependencies; parallel-safe (-j).
$(BUILD_DIR)/%.o: %.cc
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(DEP)

test check: all
	./tests/run_tests.sh $(BIN)

clean:
	rm -f $(BIN) $(OBJ) $(DEP)
	-rmdir $(BUILD_DIR)/src $(BUILD_DIR) 2>/dev/null

.PHONY: all test check clean
