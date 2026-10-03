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

SRCS := src/main.cpp src/mem.cpp src/elf_loader.cpp src/syscall.cpp src/decode_exec.cpp src/core_dump.cpp
OBJS := $(SRCS:.cpp=.o)

.PHONY: all clean test

all: zegarmistrz

zegarmistrz: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(ZYDIS_LIBS)

src/%.o: src/%.cpp $(wildcard src/*.h)
	$(CXX) $(CXXFLAGS) $(ZYDIS_CFLAGS) -c -o $@ $<

# Headers change struct layouts; objects must rebuild.
src/main.o src/mem.o src/elf_loader.o src/syscall.o src/decode_exec.o: $(wildcard src/*.h)

clean:
	rm -f zegarmistrz $(OBJS)
	rm -rf test-output zegarmistrz.core.*

test: zegarmistrz
	./run-tests
