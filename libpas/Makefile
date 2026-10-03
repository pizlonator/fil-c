# Copyright (c) 2024-2026 Epic Games, Inc. All Rights Reserved.
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

FILC_OUTPUT_ROOT ?= $(abspath ../pizfix)

FILC_LIB_DIR ?= $(FILC_OUTPUT_ROOT)/lib
FILC_LIB_TEST_DIR ?= $(FILC_OUTPUT_ROOT)/lib_test
FILC_LIB_GCVERIFY_DIR ?= $(FILC_OUTPUT_ROOT)/lib_gcverify
FILC_LIB_TEST_GCVERIFY_DIR ?= $(FILC_OUTPUT_ROOT)/lib_test_gcverify

FILC_YOLO_INCLUDE ?= $(FILC_OUTPUT_ROOT)/yolo-include
FILC_OS_INCLUDE ?= $(FILC_OUTPUT_ROOT)/os-include
FILC_STDFIL_INCLUDE ?= $(FILC_OUTPUT_ROOT)/stdfil-include
FILC_INCLUDE_DIR ?= $(FILC_OUTPUT_ROOT)/include
FILC_YOLO_LIB_DIR ?= $(FILC_LIB_DIR)
FILC_DYNAMIC_LINKER ?= $(FILC_YOLO_LIB_DIR)/ld-fil1-`uname -m`.so
FILC_RUNTIME_LIBS ?= -lyoloc -lyolom

ARCH ?= $(shell uname -m)

# Cross builds (today: FILCARCH=aarch64 on an x86_64 host, driven by
# build_yolocosmo.sh) parameterize everything below on FILCARCH: the
# artifacts land in pizfix/lib-<arch> and the objects in build-<arch>, so
# they never collide with the host-arch build.  The default (FILCARCH equal
# to the host architecture) behaves exactly as it did before.
FILCARCH ?= $(ARCH)

ifneq ($(FILCARCH),$(ARCH))

# A plain assignment here on purpose: it overrides the ?= default above
# (which won the first time it was evaluated), while an explicit FILC_LIB_DIR
# on the command line still wins over file assignments.
FILC_LIB_DIR = $(FILC_OUTPUT_ROOT)/lib-$(FILCARCH)
BUILDDIR = build-$(FILCARCH)
CROSS_TARGET_FLAGS = --target=$(FILCARCH)-linux-gnu

else

BUILDDIR = build
CROSS_TARGET_FLAGS =

endif

# Cosmo mode: when pizfix/lib/libyolocosmo.a exists (installed by
# build_yolocosmo.sh), libpizlo is built against cosmopolitan libc (the
# "yolocosmo" flavor) instead of musl. Cosmo is a statically-linked libc, so
# in cosmo mode we only build the static libpizlo.a plus filc_crt.o and
# filc_mincrt.o; the libpizlo.so variants are skipped entirely. When COSMO is
# empty, the musl (or glibc) behavior is unchanged.
COSMO := $(wildcard $(FILC_LIB_DIR)/libyolocosmo.a)

# In a cross cosmo build there is no pizfix/os-include for the target
# architecture (os-include's asm/ symlink is per-host-arch), so use the
# target's kernel headers from the cross toolchain for <asm/*> and <linux/*>.
ifneq ($(CROSS_TARGET_FLAGS),)
ifneq ($(COSMO),)
FILC_OS_INCLUDE := /usr/$(FILCARCH)-linux-gnu/include
endif
endif

ifneq ($(COSMO),)
COSMO_CCFLAGS = \
	-include $(FILC_YOLO_INCLUDE)/normalize.inc \
	-D__COSMOPOLITAN__ \
	-D_COSMO_SOURCE=1 \
	-DPAS_COSMO=1
endif

# The ISA flags follow the target architecture, not the host (for cross
# builds FILCARCH differs from ARCH; the default is unchanged).  On aarch64,
# cosmo keeps its TIB in x28 and its platform register is x18, so nothing in
# the process may use those registers as scratch - every yolo-side object
# (the pas runtime AND the Fil-C-compiled filc/src) gets the same -ffixed
# flags the cosmo build and the pizlonated user libc use, because any of
# them can call into a cosmo function that reads the TIB from x28.
#
# This is a costly codegen restriction (two registers taken away from the
# register allocator), so it is applied only in cosmo mode: COSMO is the
# per-arch libyolocosmo.a marker probe above (pizfix/lib-<arch>/ for cross
# builds, pizfix/lib/ for the host arch), so a non-cosmo aarch64 build of
# libpas keeps the default register allocation.
ifeq ($(FILCARCH),x86_64)
ARCH_CFLAGS = -march=x86-64-v2
FILCC_ARCH_FLAGS =
else ifeq ($(FILCARCH),aarch64)
ifneq ($(COSMO),)
ARCH_CFLAGS = -ffixed-x18 -ffixed-x28
FILCC_ARCH_FLAGS = -ffixed-x18 -ffixed-x28
else
ARCH_CFLAGS =
FILCC_ARCH_FLAGS =
endif
else
ARCH_CFLAGS =
FILCC_ARCH_FLAGS =
endif

# We build the runtime with clang by default because doing so produces ~1.2% better performance on
# PizBench9019. But building with g++ is supported.
# HOST_CLANG ?= g++
# HOST_CLANG_EXTRA_FLAGS = -Wno-expansion-to-defined -Wno-pragmas \
# 	-Wno-address-of-packed-member -Wno-missing-field-initializers \
# 	-isystem `./find_gcc_include_dir.rb`
HOST_CLANG ?= clang
HOST_CLANG_EXTRA_FLAGS = -isystem `./find_clang_include_dir.rb`

FILC_CLANG ?= ../build/bin/clang
FILC_DRIVER_FLAGS ?=

ifeq ($(COSMO),)
PICFLAG = -fPIC
else
# In cosmo mode the final programs are statically linked non-PIE, and cosmo
# doesn't provide __tls_get_addr, so build the pas objects without -fPIC;
# that gives __thread variables the local-exec TLS model (direct %fs:offset
# addressing), which is exactly what cosmo's TIB supports.
PICFLAG =
endif

PASCC = $(HOST_CLANG) $(CROSS_TARGET_FLAGS) $(ARCH_CFLAGS) $(PICFLAG) -pthread -nostdinc \
	-isystem $(FILC_YOLO_INCLUDE) -isystem $(FILC_OS_INCLUDE) \
	-isystem $(FILC_STDFIL_INCLUDE) \
	$(HOST_CLANG_EXTRA_FLAGS) $(COSMO_CCFLAGS)
PASASM = $(HOST_CLANG) $(CROSS_TARGET_FLAGS) $(ARCH_CFLAGS) $(PICFLAG)
FILCC = $(FILC_CLANG) $(CROSS_TARGET_FLAGS) $(FILCC_ARCH_FLAGS) $(FILC_DRIVER_FLAGS)
SHLD = $(HOST_CLANG) -Wl,-dynamic-linker,$(FILC_DYNAMIC_LINKER) \
	-nostdlib -shared $(FILC_YOLO_LIB_DIR)/crti.o \
	$(FILC_LIB_DIR)/crtbegin.o -L$(FILC_YOLO_LIB_DIR)
SHLDPOST = $(FILC_LIB_DIR)/libyolort.a $(FILC_RUNTIME_LIBS) \
	$(FILC_LIB_DIR)/crtend.o $(FILC_YOLO_LIB_DIR)/crtn.o

DIRS_TO_CREATE = \
	$(BUILDDIR) \
	$(FILC_LIB_DIR) \
	$(FILC_LIB_TEST_DIR) \
	$(FILC_LIB_GCVERIFY_DIR) \
	$(FILC_LIB_TEST_GCVERIFY_DIR)

.PHONY: all clean

ifeq ($(COSMO),)

all: \
	$(FILC_LIB_DIR)/libpizlo.so $(FILC_LIB_TEST_DIR)/libpizlo.so \
	$(FILC_LIB_GCVERIFY_DIR)/libpizlo.so $(FILC_LIB_TEST_GCVERIFY_DIR)/libpizlo.so \
	$(FILC_LIB_DIR)/libpizlo.a \
	$(FILC_LIB_DIR)/filc_crt.o $(FILC_LIB_DIR)/filc_mincrt.o

else

all: \
	$(FILC_LIB_DIR)/libpizlo.a \
	$(FILC_LIB_DIR)/filc_crt.o $(FILC_LIB_DIR)/filc_mincrt.o

endif

$(DIRS_TO_CREATE):
	mkdir -p $@

include common.mk

MAINCFLAGS = -g -O3 -W -Werror -fno-strict-aliasing
FILCFLAGS = -O3 -g -W -Werror -MD

MAINSRC = ../filc/main/main.c
FILSRCS = $(wildcard ../filc/src/*.c)

PASPIZLODYNAMICOBJSBASE = \
	$(patsubst %.c,$(BUILDDIR)/pas-pizlo-release-%.o,$(notdir $(PASSRCS))) \
	$(patsubst %.S,$(BUILDDIR)/pas-pizlo-release-%.o,$(notdir $(PASASMSRCS))) \
	$(BUILDDIR)/pas-pizlo-release-filc_dynamic.o
PASPIZLOSTATICOBJSBASE = \
	$(patsubst %.c,$(BUILDDIR)/pas-pizlo-release-%.o,$(notdir $(PASSRCS))) \
	$(patsubst %.S,$(BUILDDIR)/pas-pizlo-release-%.o,$(notdir $(PASASMSRCS))) \
	$(BUILDDIR)/pas-pizlo-release-filc_static.o
PASPIZLOTESTOBJSBASE = \
	$(patsubst %.c,$(BUILDDIR)/pas-pizlo-test-%.o,$(notdir $(PASSRCS))) \
	$(patsubst %.S,$(BUILDDIR)/pas-pizlo-release-%.o,$(notdir $(PASASMSRCS))) \
	$(BUILDDIR)/pas-pizlo-test-filc_dynamic.o

PASPIZLODYNAMICOBJS = \
	$(PASPIZLODYNAMICOBJSBASE) \
	$(BUILDDIR)/pas-pizlo-release-filc_default_settings.o
PASPIZLODYNAMICGCVERIFYOBJS = \
	$(PASPIZLODYNAMICOBJSBASE) \
	$(BUILDDIR)/pas-pizlo-release-filc_gc_verify_settings.o
PASPIZLOSTATICOBJS = \
	$(PASPIZLOSTATICOBJSBASE) \
	$(BUILDDIR)/pas-pizlo-release-filc_default_settings.o
PASPIZLOTESTOBJS = \
	$(PASPIZLOTESTOBJSBASE) \
	$(BUILDDIR)/pas-pizlo-test-filc_default_settings.o
PASPIZLOTESTGCVERIFYOBJS = \
	$(PASPIZLOTESTOBJSBASE) \
	$(BUILDDIR)/pas-pizlo-test-filc_gc_verify_settings.o

FILPIZLOOBJS = $(patsubst %.c,$(BUILDDIR)/fil-pizlo-%.o,$(notdir $(FILSRCS)))

-include $(wildcard $(BUILDDIR)/*.d)

clean:
	rm -rf $(BUILDDIR)
	rm -f src/libpas/filc_native_forwarders.c
	rm -f src/libpas/filc_native.h
	rm -f $(FILC_LIB_DIR)/filc_crt.o
	rm -f $(FILC_LIB_DIR)/filc_mincrt.o
	rm -f $(FILC_LIB_DIR)/libpizlo.so
	rm -f $(FILC_LIB_TEST_DIR)/libpizlo.so
	rm -f $(FILC_LIB_DIR)/libpizlo.a

.filc-objs: $(FILPIZLOOBJS)
.yoloc-dynamic-objs: $(PASPIZLODYNAMICOBJS)
.yoloc-dynamic-gcverify-objs: $(PASPIZLODYNAMICGCVERIFYOBJS)
.yoloc-test-objs: $(PASPIZLOTESTOBJS)
.yoloc-test-gcverify-objs: $(PASPIZLOTESTGCVERIFYOBJS)
.yoloc-static-objs: $(PASPIZLOSTATICOBJS)

$(FILC_LIB_DIR)/libpizlo.so: $(PASPIZLODYNAMICOBJS) $(FILPIZLOOBJS) | $(FILC_LIB_DIR)
	$(SHLD) -o $@ $(PASPIZLODYNAMICOBJS) $(FILPIZLOOBJS) $(SHLDPOST)

$(FILC_LIB_GCVERIFY_DIR)/libpizlo.so: $(PASPIZLODYNAMICGCVERIFYOBJS) $(FILPIZLOOBJS) | $(FILC_LIB_GCVERIFY_DIR)
	$(SHLD) -o $@ $(PASPIZLODYNAMICGCVERIFYOBJS) $(FILPIZLOOBJS) \
		$(SHLDPOST)

$(FILC_LIB_TEST_DIR)/libpizlo.so: $(PASPIZLOTESTOBJS) $(FILPIZLOOBJS) | $(FILC_LIB_TEST_DIR)
	$(SHLD) -o $@ $(PASPIZLOTESTOBJS) $(FILPIZLOOBJS) $(SHLDPOST)

$(FILC_LIB_TEST_GCVERIFY_DIR)/libpizlo.so: $(PASPIZLOTESTGCVERIFYOBJS) $(FILPIZLOOBJS) | $(FILC_LIB_TEST_GCVERIFY_DIR)
	$(SHLD) -o $@ $(PASPIZLOTESTGCVERIFYOBJS) \
		$(FILPIZLOOBJS) $(SHLDPOST)

$(FILC_LIB_DIR)/libpizlo.a: $(PASPIZLOSTATICOBJS) $(FILPIZLOOBJS) | $(FILC_LIB_DIR)
	rm -f $@
	ar cr $@ $(PASPIZLOSTATICOBJS) $(FILPIZLOOBJS)

$(BUILDDIR)/pas-pizlo-release-%.o: src/libpas/%.c | $(BUILDDIR)
	$(PASCC) $(PASCFLAGS) -c -o $@ $< -DPAS_FILC=1

$(BUILDDIR)/pas-pizlo-test-%.o: src/libpas/%.c | $(BUILDDIR)
	$(PASCC) $(PASCFLAGS) -c -o $@ $< -DPAS_FILC=1 -DENABLE_PAS_TESTING=1

$(BUILDDIR)/fil-pizlo-%.o: ../filc/src/%.c $(FILC_CLANG) | $(BUILDDIR)
	$(FILCC) $(FILCFLAGS) -c -o $@ $<

$(BUILDDIR)/pas-pizlo-release-%.o: src/libpas/%.S | $(BUILDDIR)
	$(PASASM) $(PASASMFLAGS) -c -o $@ $<

$(BUILDDIR)/pas-pizlo-release-filc_runtime.o: src/libpas/filc_native.h
$(BUILDDIR)/pas-pizlo-test-filc_runtime.o: src/libpas/filc_native.h
$(BUILDDIR)/pas-test-filc_runtime.o: src/libpas/filc_native.h
$(BUILDDIR)/pas-pizlo-release-filc_start_program.o: src/libpas/filc_native.h
$(BUILDDIR)/pas-pizlo-test-filc_start_program.o: src/libpas/filc_native.h
$(BUILDDIR)/pas-test-filc_start_program.o: src/libpas/filc_native.h

$(FILC_LIB_DIR)/filc_crt.o: $(MAINSRC) | $(FILC_LIB_DIR)
	$(PASCC) -c -o $@ $(MAINSRC) $(MAINCFLAGS) -DUSE_LIBC=1

$(FILC_LIB_DIR)/filc_mincrt.o: $(MAINSRC) | $(FILC_LIB_DIR)
	$(PASCC) -c -o $@ $(MAINSRC) $(MAINCFLAGS) -DUSE_LIBC=0

