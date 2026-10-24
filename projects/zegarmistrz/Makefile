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

# zegarmistrz -- x86-64 user-mode emulator

CXX ?= g++
CXXFLAGS ?= -std=c++20 -O2 -g -Wall -D_GNU_SOURCE
LDFLAGS ?= -pthread

# Zydis comes from the system (apt install libzydis-dev libzycore-dev).
ZYDIS_CFLAGS := $(shell (pkg-config --cflags zydis 2>/dev/null || echo "-I/usr/include"))
ZYDIS_LIBS := $(shell (pkg-config --libs zydis 2>/dev/null || echo "-lZydis -lZycore"))

# BFD (binutils-dev) symbolizes guest addresses for crash-time stack
# traces. Header and library are paired by ABSOLUTE PATH (the literal .so
# file goes on the link line): a bare -lbfd would let ld's default search
# dirs (which include /usr/local/lib) silently bind a from-source
# /usr/local/lib/libbfd.so to the system header -- that compiles fine but
# symbolizes nothing at runtime, exactly the mismatch this avoids. The
# forced include by absolute path wins over the default header search
# order (where /usr/local/include comes first); both headers share the
# __BFD_H_SEEN__ guard, so the later <bfd.h> include in stack_trace.cpp is
# a no-op. (-isystem /usr/include is NOT usable here: it reorders the C++
# header chain and breaks libstdc++'s #include_next <stdlib.h>.) Probed in
# order: the compiler's multiarch system install ($(CXX) -print-multiarch;
# with a glob across /usr/lib/<triplet>/ as a fallback, so aarch64 and
# friends work), a plain /usr/lib/libbfd.so, and a /usr/local from-source
# install (its own header). The literal path records the library's own
# SONAME, so runtime resolution is unchanged.
BFD_SYS_CFLAGS := -include /usr/include/bfd.h -DPACKAGE='"zegarmistrz"' -DPACKAGE_VERSION='"1"'
BFD_LOCAL_CFLAGS := -include /usr/local/include/bfd.h -DPACKAGE='"zegarmistrz"' -DPACKAGE_VERSION='"1"'
BFD_MULTIARCH := $(shell $(CXX) -print-multiarch 2>/dev/null)
BFD_SYS_LIBS := $(if $(BFD_MULTIARCH),$(wildcard /usr/lib/$(BFD_MULTIARCH)/libbfd.so)) \
                $(wildcard /usr/lib/*/libbfd.so) \
                $(wildcard /usr/lib/libbfd.so)
BFD_SYS_LIB := $(firstword $(BFD_SYS_LIBS))
BFD_LOCAL_LIB := /usr/local/lib/libbfd.so
H := \#
BFD_TMP := $(shell mktemp /tmp/zegarmistrz.bfdprobe.XXXXXX.cpp)
$(file >$(BFD_TMP),$(H)include <bfd.h>)
$(file >>$(BFD_TMP),int main(){bfd_init();return 0;})
BFD_CFLAGS :=
BFD_LIBS :=
ifneq ($(strip $(BFD_SYS_LIB)),)
ifeq ($(shell $(CXX) $(CXXFLAGS) $(BFD_SYS_CFLAGS) $(BFD_TMP) -o /dev/null $(BFD_SYS_LIB) 2>/dev/null && echo yes),yes)
BFD_CFLAGS := $(BFD_SYS_CFLAGS)
BFD_LIBS := $(BFD_SYS_LIB)
endif
endif
ifeq ($(strip $(BFD_CFLAGS)),)
ifeq ($(shell test -f $(BFD_LOCAL_LIB) && $(CXX) $(CXXFLAGS) $(BFD_LOCAL_CFLAGS) $(BFD_TMP) -o /dev/null $(BFD_LOCAL_LIB) 2>/dev/null && echo yes),yes)
BFD_CFLAGS := $(BFD_LOCAL_CFLAGS)
BFD_LIBS := $(BFD_LOCAL_LIB)
endif
endif
ifeq ($(strip $(BFD_CFLAGS)),)
$(shell rm -f $(BFD_TMP))
$(error BFD not found: install binutils-dev, e.g. sudo apt-get install -y binutils-dev)
endif
$(shell rm -f $(BFD_TMP))

SRCS := src/main.cpp src/mem.cpp src/elf_loader.cpp src/syscall.cpp src/decode_exec.cpp src/core_dump.cpp src/stack_trace.cpp
OBJS := $(SRCS:.cpp=.o)

.PHONY: all clean test

all: zegarmistrz

zegarmistrz: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(ZYDIS_LIBS) $(BFD_LIBS)

src/%.o: src/%.cpp $(wildcard src/*.h)
	$(CXX) $(CXXFLAGS) $(ZYDIS_CFLAGS) -c -o $@ $<

# Only stack_trace.cpp includes bfd.h (and needs its PACKAGE macros).
src/stack_trace.o: src/stack_trace.cpp $(wildcard src/*.h)
	$(CXX) $(CXXFLAGS) $(ZYDIS_CFLAGS) $(BFD_CFLAGS) -c -o $@ $<

# Headers change struct layouts; objects must rebuild.
src/main.o src/mem.o src/elf_loader.o src/syscall.o src/decode_exec.o: $(wildcard src/*.h)

clean:
	rm -f zegarmistrz $(OBJS)
	rm -rf test-output zegarmistrz.core.*

test: zegarmistrz
	./run-tests
