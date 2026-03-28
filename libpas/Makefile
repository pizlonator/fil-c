# Copyright (c) 2024-2026 Epic Games, Inc. All Rights Reserved.
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
FILC_DYNAMIC_LINKER ?= $(FILC_YOLO_LIB_DIR)/ld-yolo-x86_64.so
FILC_RUNTIME_LIBS ?= -lyoloc -lyolom

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

PASCC = $(HOST_CLANG) -march=x86-64-v2 -fPIC -pthread -nostdinc \
	-isystem $(FILC_YOLO_INCLUDE) -isystem $(FILC_OS_INCLUDE) \
	-isystem $(FILC_STDFIL_INCLUDE) \
	$(HOST_CLANG_EXTRA_FLAGS)
PASASM = $(HOST_CLANG) -march=x86-64-v2 -fPIC
FILCC = $(FILC_CLANG) $(FILC_DRIVER_FLAGS)
SHLD = $(HOST_CLANG) -Wl,-dynamic-linker,$(FILC_DYNAMIC_LINKER) \
	-nostdlib -shared $(FILC_YOLO_LIB_DIR)/crti.o \
	$(FILC_LIB_DIR)/crtbegin.o -L$(FILC_YOLO_LIB_DIR)
SHLDPOST = $(FILC_LIB_DIR)/libyolort.a $(FILC_RUNTIME_LIBS) \
	$(FILC_LIB_DIR)/crtend.o $(FILC_YOLO_LIB_DIR)/crtn.o

DIRS_TO_CREATE = \
	$(FILC_LIB_DIR) \
	$(FILC_LIB_TEST_DIR) \
	$(FILC_LIB_GCVERIFY_DIR) \
	$(FILC_LIB_TEST_GCVERIFY_DIR)

.PHONY: all clean

all: \
	$(FILC_LIB_DIR)/libpizlo.so $(FILC_LIB_TEST_DIR)/libpizlo.so \
	$(FILC_LIB_GCVERIFY_DIR)/libpizlo.so $(FILC_LIB_TEST_GCVERIFY_DIR)/libpizlo.so \
	$(FILC_LIB_DIR)/libpizlo.a \
	$(FILC_LIB_DIR)/filc_crt.o $(FILC_LIB_DIR)/filc_mincrt.o

$(DIRS_TO_CREATE):
	mkdir -p $@

include common.mk

MAINCFLAGS = -g -O3 -W -Werror -fno-strict-aliasing
FILCFLAGS = -O3 -g -W -Werror -MD

MAINSRC = ../filc/main/main.c
FILSRCS = $(wildcard ../filc/src/*.c)

PASPIZLODYNAMICOBJSBASE = \
	$(patsubst %.c,build/pas-pizlo-release-%.o,$(notdir $(PASSRCS))) \
	$(patsubst %.s,build/pas-pizlo-release-%.o,$(notdir $(PASASMSRCS))) \
	build/pas-pizlo-release-filc_dynamic.o
PASPIZLOSTATICOBJSBASE = \
	$(patsubst %.c,build/pas-pizlo-release-%.o,$(notdir $(PASSRCS))) \
	$(patsubst %.s,build/pas-pizlo-release-%.o,$(notdir $(PASASMSRCS))) \
	build/pas-pizlo-release-filc_static.o
PASPIZLOTESTOBJSBASE = \
	$(patsubst %.c,build/pas-pizlo-test-%.o,$(notdir $(PASSRCS))) \
	$(patsubst %.s,build/pas-pizlo-release-%.o,$(notdir $(PASASMSRCS))) \
	build/pas-pizlo-test-filc_dynamic.o

PASPIZLODYNAMICOBJS = \
	$(PASPIZLODYNAMICOBJSBASE) \
	build/pas-pizlo-release-filc_default_settings.o
PASPIZLODYNAMICGCVERIFYOBJS = \
	$(PASPIZLODYNAMICOBJSBASE) \
	build/pas-pizlo-release-filc_gc_verify_settings.o
PASPIZLOSTATICOBJS = \
	$(PASPIZLOSTATICOBJSBASE) \
	build/pas-pizlo-release-filc_default_settings.o
PASPIZLOTESTOBJS = \
	$(PASPIZLOTESTOBJSBASE) \
	build/pas-pizlo-test-filc_default_settings.o
PASPIZLOTESTGCVERIFYOBJS = \
	$(PASPIZLOTESTOBJSBASE) \
	build/pas-pizlo-test-filc_gc_verify_settings.o

FILPIZLOOBJS = $(patsubst %.c,build/fil-pizlo-%.o,$(notdir $(FILSRCS)))

-include $(wildcard build/*.d)

clean:
	rm -rf build
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

build/pas-pizlo-release-%.o: src/libpas/%.c
	$(PASCC) $(PASCFLAGS) -c -o $@ $< -DPAS_FILC=1

build/pas-pizlo-test-%.o: src/libpas/%.c
	$(PASCC) $(PASCFLAGS) -c -o $@ $< -DPAS_FILC=1 -DENABLE_PAS_TESTING=1

build/fil-pizlo-%.o: ../filc/src/%.c $(FILC_CLANG)
	$(FILCC) $(FILCFLAGS) -c -o $@ $<

build/pas-pizlo-release-%.o: src/libpas/%.s
	$(PASASM) $(PASASMFLAGS) -c -o $@ $<

build/pas-pizlo-release-filc_runtime.o: src/libpas/filc_native.h
build/pas-pizlo-test-filc_runtime.o: src/libpas/filc_native.h
build/pas-test-filc_runtime.o: src/libpas/filc_native.h
build/pas-pizlo-release-filc_start_program.o: src/libpas/filc_native.h
build/pas-pizlo-test-filc_start_program.o: src/libpas/filc_native.h
build/pas-test-filc_start_program.o: src/libpas/filc_native.h

$(FILC_LIB_DIR)/filc_crt.o: $(MAINSRC) | $(FILC_LIB_DIR)
	$(PASCC) -c -o $@ $(MAINSRC) $(MAINCFLAGS) -DUSE_LIBC=1

$(FILC_LIB_DIR)/filc_mincrt.o: $(MAINSRC) | $(FILC_LIB_DIR)
	$(PASCC) -c -o $@ $(MAINSRC) $(MAINCFLAGS) -DUSE_LIBC=0

src/libpas/filc_native.h: src/libpas/generate_pizlonated_forwarders.rb
	ruby src/libpas/generate_pizlonated_forwarders.rb src/libpas/filc_native.h
src/libpas/filc_native_forwarders.c: src/libpas/generate_pizlonated_forwarders.rb
	ruby src/libpas/generate_pizlonated_forwarders.rb src/libpas/filc_native_forwarders.c
