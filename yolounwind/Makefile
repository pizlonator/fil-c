# Copyright (c) 2025 Epic Games, Inc. All Rights Reserved.
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
# THIS SOFTWARE IS PROVIDED BY EPIC GAMES, INC. ``AS IS'' AND ANY
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

# The unwinder is one portable C file.  Cross builds (FILCARCH=aarch64, from
# build_yolocosmo.sh) put their artifacts in per-arch file names so they don't
# collide with the host-arch build; the default (FILCARCH = host) is unchanged.
FILCARCH ?= $(shell uname -m)

# Cosmo mode probe, mirroring libpas/Makefile: the cross cosmo build is
# detected by the per-arch libyolocosmo.a marker that build_yolocosmo.sh
# installs into pizfix/lib-<arch> (pizfix/lib/ for the host arch).  This is
# only consulted to decide whether costly cosmo-only codegen restrictions
# apply; a non-cosmo build of the unwinder is unaffected.
ifeq ($(FILCARCH),$(shell uname -m))
COSMO_LIB_DIR = ../pizfix/lib
else
COSMO_LIB_DIR = ../pizfix/lib-$(FILCARCH)
endif
COSMO := $(wildcard $(COSMO_LIB_DIR)/libyolocosmo.a)

ifneq ($(FILCARCH),$(shell uname -m))
ARCHSUFFIX = -$(FILCARCH)
CROSS_TARGET_FLAGS = --target=$(FILCARCH)-linux-gnu
# The aarch64 cross build (for the cosmo flavor) must not use x18 (the
# aarch64 platform register) or x28 (cosmo's TIB register) as scratch: the
# unwinder runs beneath pizlonated frames and calls the pizlonated
# personality function and cleanup handlers mid-unwind, and the yolo layer
# reads cosmo's TIB from x28.  This matches the -ffixed-x18 -ffixed-x28
# used by libpas, the pizlonated libc and the pizlonated C++ runtime for
# the same architecture (see build_yolocosmo.sh, projects/usercosmo/filc.mk
# and libpas/Makefile).  Reserving registers is a costly codegen
# restriction, so it applies only in cosmo mode (the per-arch cosmo marker
# above); a non-cosmo aarch64 build keeps the default register allocation.
ifneq ($(findstring aarch64,$(FILCARCH)),)
ifneq ($(COSMO),)
CROSS_TARGET_FLAGS += -ffixed-x18 -ffixed-x28
endif
endif
else
ARCHSUFFIX =
CROSS_TARGET_FLAGS =
endif

all: libyolounwind$(ARCHSUFFIX).a

clean:
	rm -f libyolounwind.a libyolounwind$(ARCHSUFFIX).a yolounwind.o yolounwind$(ARCHSUFFIX).o

libyolounwind$(ARCHSUFFIX).a: yolounwind$(ARCHSUFFIX).o
	ar cr libyolounwind$(ARCHSUFFIX).a yolounwind$(ARCHSUFFIX).o

yolounwind$(ARCHSUFFIX).o: yolounwind.c
	clang $(CROSS_TARGET_FLAGS) -c -o yolounwind$(ARCHSUFFIX).o yolounwind.c -O2 -g
