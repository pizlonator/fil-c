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

all: \
	../pizfix/lib/libpizlo.so ../pizfix/lib_test/libpizlo.so ../pizfix/lib/libpizlo.a \
	../pizfix/lib/filc_crt.o ../pizfix/lib/filc_mincrt.o

PASCC = clang -march=x86-64-v2 -fPIC -pthread -nostdinc -isystem ../pizfix/yolo-include \
	-isystem ../pizfix/os-include -isystem `./find_clang_include_dir.rb`
PASASM = clang -march=x86-64-v2 -fPIC
FILCC = ../build/bin/clang
SHLD = clang -Wl,-dynamic-linker,$(PWD)/../pizfix/lib/ld-yolo-x86_64.so \
	-nostdlib -shared ../pizfix/lib/crti.o \
	`./find_system_library_file.rb crtbeginS.o` -L../pizfix/lib
SHLDPOST = `./find_system_library_file.rb libgcc.a` -lyoloc -lyolom \
	`./find_system_library_file.rb crtendS.o` ../pizfix/lib/crtn.o

include common.mk

MAINCFLAGS = -g -O3 -W -Werror -fno-strict-aliasing
FILCFLAGS = -O3 -g -W -Werror -MD

MAINSRC = ../filc/main/main.c
FILSRCS = $(wildcard ../filc/src/*.c)

PASPIZLODYNAMICOBJS = \
	$(patsubst %.c,build/pas-pizlo-release-%.o,$(notdir $(PASSRCS))) \
	$(patsubst %.s,build/pas-pizlo-release-%.o,$(notdir $(PASASMSRCS))) \
	build/pas-pizlo-test-filc_dynamic.o
PASPIZLOSTATICOBJS = \
	$(patsubst %.c,build/pas-pizlo-release-%.o,$(notdir $(PASSRCS))) \
	$(patsubst %.s,build/pas-pizlo-release-%.o,$(notdir $(PASASMSRCS))) \
	build/pas-pizlo-test-filc_static.o
PASPIZLOTESTOBJS = \
	$(patsubst %.c,build/pas-pizlo-test-%.o,$(notdir $(PASSRCS))) \
	$(patsubst %.s,build/pas-pizlo-release-%.o,$(notdir $(PASASMSRCS))) \
	build/pas-pizlo-test-filc_dynamic.o

FILPIZLOOBJS = $(patsubst %.c,build/fil-pizlo-%.o,$(notdir $(FILSRCS)))

-include $(wildcard build/*.d)

clean:
	rm -rf build
	rm -f src/libpas/filc_native_forwarders.c
	rm -f src/libpas/filc_native.h
	rm -f ../pizfix/lib/filc_crt.o
	rm -f ../pizfix/lib/filc_mincrt.o
	rm -f ../pizfix/lib/libpizlo.so
	rm -f ../pizfix/lib_test/libpizlo.so
	rm -f ../pizfix/lib/libpizlo.a

.filc-objs: $(FILPIZLOOBJS)
.yoloc-dynamic-objs: $(PASPIZLODYNAMICOBJS)
.yoloc-static-objs: $(PASPIZLOSTATICOBJS)

../pizfix/lib/libpizlo.so: $(PASPIZLODYNAMICOBJS) $(FILPIZLOOBJS)
	$(SHLD) -o ../pizfix/lib/libpizlo.so $(PASPIZLODYNAMICOBJS) $(FILPIZLOOBJS) $(SHLDPOST)

../pizfix/lib_test/libpizlo.so: $(PASPIZLOTESTOBJS) $(FILPIZLOOBJS)
	$(SHLD) -o ../pizfix/lib_test/libpizlo.so $(PASPIZLOTESTOBJS) $(FILPIZLOOBJS) $(SHLDPOST)

../pizfix/lib/libpizlo.a: $(PASPIZLOSTATICOBJS) $(FILPIZLOOBJS)
	rm -f ../pizfix/lib/libpizlo.a
	ar cr ../pizfix/lib/libpizlo.a $(PASPIZLOSTATICOBJS) $(FILPIZLOOBJS)

build/pas-pizlo-release-%.o: src/libpas/%.c
	$(PASCC) $(PASCFLAGS) -c -o $@ $< -DPAS_FILC=1

build/pas-pizlo-test-%.o: src/libpas/%.c
	$(PASCC) $(PASCFLAGS) -c -o $@ $< -DPAS_FILC=1 -DENABLE_PAS_TESTING=1

build/fil-pizlo-%.o: ../filc/src/%.c ../build/bin/clang
	$(FILCC) $(FILCFLAGS) -c -o $@ $<

build/pas-pizlo-release-%.o: src/libpas/%.s
	$(PASASM) $(PASASMFLAGS) -c -o $@ $<

build/pas-pizlo-release-filc_runtime.o: src/libpas/filc_native.h
build/pas-pizlo-test-filc_runtime.o: src/libpas/filc_native.h
build/pas-test-filc_runtime.o: src/libpas/filc_native.h
build/pas-pizlo-release-filc_start_program.o: src/libpas/filc_native.h
build/pas-pizlo-test-filc_start_program.o: src/libpas/filc_native.h
build/pas-test-filc_start_program.o: src/libpas/filc_native.h

../pizfix/lib/filc_crt.o: $(MAINSRC)
	$(PASCC) -c -o ../pizfix/lib/filc_crt.o $(MAINSRC) $(MAINCFLAGS) -DUSE_LIBC=1

../pizfix/lib/filc_mincrt.o: $(MAINSRC)
	$(PASCC) -c -o ../pizfix/lib/filc_mincrt.o $(MAINSRC) $(MAINCFLAGS) -DUSE_LIBC=0

src/libpas/filc_native.h: src/libpas/generate_pizlonated_forwarders.rb
	ruby src/libpas/generate_pizlonated_forwarders.rb src/libpas/filc_native.h
src/libpas/filc_native_forwarders.c: src/libpas/generate_pizlonated_forwarders.rb
	ruby src/libpas/generate_pizlonated_forwarders.rb src/libpas/filc_native_forwarders.c


