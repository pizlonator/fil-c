#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#── vi: set et ft=make ts=8 sw=8 fenc=utf-8 :vi ──────────────────────┘
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
#
# Builds "usercosmo": cosmopolitan libc compiled BY the Fil-C compiler.
#
# This is the cosmo-flavor counterpart of the musl user libc.  It compiles a
# curated subset of cosmo's C sources with build/bin/clang (which pizlonates
# everything), producing:
#
#   pizfix/lib/libc.a   the pizlonated cosmo libc (static); cross builds
#                       (FILCARCH=aarch64) install the same set of artifacts
#                       into pizfix/lib-aarch64 instead
#   pizfix/lib/libm.a   an empty archive so that -lm resolves (cosmo folds
#                       all of libm into libc.a)
#   pizfix/lib/crt1.o   the process entry object (yolo cosmo crt + yolo glue)
#   pizfix/include/     cosmo public headers (the flavor switch for the
#                       driver; only the host-arch build installs them since
#                       cosmo's headers are arch-neutral).  Only the headers
#                       of code that is actually part of this libc get
#                       installed: the isystem public headers minus the
#                       wrappers for cosmo's vendored C++-runtime/openmp/
#                       libunwind trees (the Fil-C world uses LLVM
#                       libc++/libc++abi and stdfil-include/unwind.h), plus
#                       the internal header trees that the installed public
#                       headers actually reach.  See section 7 below.
#
# This makefile is driven by build_usercosmo.sh, which runs:
#
#   make -C projects/usercosmo -f filc.mk -j $NCPU install \
#       PIZFIX=$ROOT/pizfix FILC_CLANG=$ROOT/build/bin/clang
#
# Preconditions (the `install` target checks them, see check-env below):
#   1. build/bin/clang exists (Fil-C compiler)
#   2. ./build_yolocosmo.sh has run (installs libyolocosmo.a + ape.o + ape.lds
#      + cosmo-crt.o + pizfix/yolo-include, flipping libpas into cosmo mode)
#   3. (cd libpas && ./clean.sh && ./build.sh)  (cosmo-mode libpizlo.a)
#
# Incremental builds just work (`make` recompiles what is out of date).
# Force a full rebuild with FORCE=1 - useful after editing a widely-included
# header, since header dependencies are not tracked (this mirrors the old
# build_usercosmo.sh behavior, where only source-vs-object mtimes were
# compared).
#
# Cross-building for another architecture is a matter of overriding FILCARCH
# (and FILC_BUILDDIR, so the objects don't collide); the source lists and the
# rules below are architecture-neutral.

# All paths are resolved relative to this makefile (projects/usercosmo),
# which `make -C projects/usercosmo -f filc.mk` makes the working directory.
COSMO := $(CURDIR)
ROOT := $(abspath $(COSMO)/../..)

# Target architecture (defaults to the host).  x86_64 is the host build and
# installs into pizfix/lib; aarch64 is a cross build and installs into
# pizfix/lib-aarch64 (see LIBDIR below).
FILCARCH ?= $(shell uname -m)

ifeq ($(FILCARCH),x86_64)
TARGET_FLAG =
ARCH_CFLAGS = -march=x86-64 -mavx
LIBDIR = $(PIZFIX)/lib
else ifeq ($(FILCARCH),aarch64)
TARGET_FLAG = --target=aarch64-linux-gnu
# Cosmo's aarch64 conventions (see build/definitions.mk and the cosmocross
# wrapper): char is signed, x18 is the platform register, and x28 holds the
# cosmo TIB.  -ffixed-x28 is required here even though the pizlonated libc
# mostly avoids the cosmo TLS machinery: some pizlonated sources inline
# __get_tls() (which reads x28 on aarch64), and without the fix LLVM is free
# to reuse x28 as scratch inside the same function - the read then sees
# garbage (this crashes inside __mmap_impl).
ARCH_CFLAGS = -fsigned-char -ffixed-x18 -ffixed-x28
LIBDIR = $(PIZFIX)/lib-aarch64
# x86-only raw asm (the metal BIOS I/O port readers in pc.internal.h, the
# serial console, and amd64 crash dumping); metal is an x86-only concept
# (see libc/dce.h: SupportsMetal() is 0 on aarch64).
AARCH64_EXCLUDE_FILES = \
	libc/calls/metalfile.c \
	libc/calls/openat-metal.c \
	libc/calls/poll-metal.c \
	libc/calls/readv-serial.c \
	libc/calls/writev-serial.c \
	libc/intrin/directmap-metal.c \
	libc/intrin/munmap-metal.c \
	libc/log/oncrash_amd64.c \
	libc/runtime/efimain.greg.c \
	libc/runtime/metalprintf.greg.c \
	libc/sysv/sysv.c \
	libc/nexgen32e/argv2.c
else
$(error unsupported FILCARCH "$(FILCARCH)")
endif

# Where the objects and generated sources go.  Cross builds should override
# this (e.g. `make -f filc.mk FILCARCH=aarch64 FILC_BUILDDIR=o-filc-aarch64`).
FILC_BUILDDIR ?= o-filc
BUILD := $(COSMO)/$(FILC_BUILDDIR)

# The Fil-C compiler and its builtin headers, and the install prefix.
FILC_CLANG ?= $(ROOT)/build/bin/clang
CLANG_RES ?= $(ROOT)/build/lib/clang/20/include
PIZFIX ?= $(ROOT)/pizfix

.DEFAULT_GOAL := install
.PHONY: all install check-env install-headers

# ─────────────────────────────────────────────────────────────────────────────
# 1. Compile flags.  The Fil-C driver gets -nostdinc plus explicit include
#    paths: stdfil-include (stdfil.h + pizlonated_*.h), then cosmo's own
#    headers.  SUPPORT_VECTOR=1 is the "optlinux" configuration: Linux-only,
#    which makes every Windows/XNU/BSD/Metal branch fold away at compile time
#    (crucial: those branches reference raw asm and Win32 APIs).
# ─────────────────────────────────────────────────────────────────────────────
COSMO_INC = \
 -nostdinc \
 -isystem $(PIZFIX)/stdfil-include \
 -isystem $(COSMO) \
 -isystem $(COSMO)/libc/isystem \
 -isystem $(CLANG_RES) \
 -include $(COSMO)/libc/integral/normalize.inc

CFLAGS = $(TARGET_FLAG) -O2 -g -std=gnu23 \
 -DSUPPORT_VECTOR=1 -D_COSMO_SOURCE -DNDEBUG -DMODE=\"filc\" \
 $(ARCH_CFLAGS) \
 -fno-omit-frame-pointer -fno-stack-protector -fwrapv \
 -fno-common -w \
 $(COSMO_INC)

# A few cosmo sources are C++ (.cc): libc/str/isw{lower,upper,separator}.cc
# implement the wide-char classification tables with a C++ template
# (libc/str/has_char.h).  They compile fine with the Fil-C C++ front end; the
# public declarations are extern "C" via COSMOPOLITAN_C_START_, so the
# symbols land unmangled as pizlonated_<name>.
CCXX = libc/str/iswlower.cc libc/str/iswupper.cc libc/str/iswseparator.cc

CXXFLAGS = $(TARGET_FLAG) -O2 -g -std=gnu++20 \
 -DSUPPORT_VECTOR=1 -D_COSMO_SOURCE -DNDEBUG -DMODE=\"filc\" \
 $(ARCH_CFLAGS) \
 -fno-omit-frame-pointer -fno-stack-protector -fwrapv \
 -fno-common -w -fno-exceptions -fno-rtti -nostdinc++ \
 $(COSMO_INC)

# FORCE=1 rebuilds everything (see the file header).
ifneq ($(FORCE),)
FORCE_PREREQ = filc-force
.PHONY: filc-force
filc-force:
endif

# ─────────────────────────────────────────────────────────────────────────────
# 2. The syscall shims (replacements for libc/sysv/calls/*.S).  They are
#    extracted from cosmo's own headers via the compiler's AST, so every
#    definition is type-checked against cosmo's own prototypes.  The
#    generation is a proper make step: it reruns whenever gen_shims.py, the
#    cosmo headers/sources it scans, or the pizlonated syscall ABI header
#    change.
# ─────────────────────────────────────────────────────────────────────────────
SHIM_HEADERS := $(shell sed -n '/^BASE_HEADERS = \[/,/^\]/p' \
	$(COSMO)/filc/gen_shims.py | grep -oE '"[^"]+"' | tr -d '"' | \
	sed 's|^|$(COSMO)/|')

SHIM_DEPS = \
	$(COSMO)/filc/gen_shims.py \
	$(ROOT)/filc/include/pizlonated_syscalls.h \
	$(SHIM_HEADERS) \
	$(wildcard $(COSMO)/libc/sysv/calls/*.S) \
	$(wildcard $(COSMO)/libc/sysv/consts/*.S) \
	$(wildcard $(COSMO)/libc/sysv/errfuns/*.S)

$(BUILD)/.shims.stamp: $(SHIM_DEPS)
	@mkdir -p $(BUILD)
	FILCARCH=$(FILCARCH) python3 $(COSMO)/filc/gen_shims.py $(BUILD)/shims.c $(BUILD)/consts.c
	@touch $@

$(BUILD)/generated/shims.o: $(BUILD)/.shims.stamp | check-env
	@mkdir -p $(dir $@)
	@$(FILC_CLANG) $(CFLAGS) -c -o $@ $(BUILD)/shims.c \
	    2>>$(BUILD)/compile-errors.log || { \
	    echo "FAILED: shims.c (details in $(FILC_BUILDDIR)/compile-errors.log)" >&2; \
	    exit 1; }

$(BUILD)/generated/consts.o: $(BUILD)/.shims.stamp | check-env
	@mkdir -p $(dir $@)
	@$(FILC_CLANG) $(CFLAGS) -c -o $@ $(BUILD)/consts.c \
	    2>>$(BUILD)/compile-errors.log || { \
	    echo "FAILED: consts.c (details in $(FILC_BUILDDIR)/compile-errors.log)" >&2; \
	    exit 1; }

$(BUILD)/generated/errfuns.o: $(BUILD)/.shims.stamp | check-env
	@mkdir -p $(dir $@)
	@$(FILC_CLANG) $(CFLAGS) -c -o $@ $(BUILD)/errfuns.c \
	    2>>$(BUILD)/compile-errors.log || { \
	    echo "FAILED: errfuns.c (details in $(FILC_BUILDDIR)/compile-errors.log)" >&2; \
	    exit 1; }

GEN_OBJS = \
	$(BUILD)/generated/shims.o \
	$(BUILD)/generated/consts.o \
	$(BUILD)/generated/errfuns.o

# ─────────────────────────────────────────────────────────────────────────────
# 3. The sources to compile.
#
#    Exclusions (each with a reason):
#      libc/intrin/mmap.c munmap.c mprotect.c msync.c madvise.c mincore.c
#                         cosmo's public memory APIs keep bookkeeping in the
#                         __maps RB-tree, which packs tag bits into pointers
#                         (ABA()), a capability no-no.  filc_mmap.c provides
#                         thin zsys_*-based replacements instead.
#      libc/intrin/maps.c the __maps machinery itself (ABA pointer tagging)
#      libc/intrin/stack.c, permalloc.c, describefds.c
#                         more __maps-machinery users (cosmo_stack and
#                         permalloc have Fil-C replacements/stubs)
#      libc/intrin/brain16.c float16.c   bf16/f16 compiler runtime; clang
#                         under Fil-C has no _Float32/__bf16 types and
#                         compiler-rt provides these routines anyway
#      libc/intrin/describe*.c, demangle.c, printmapswin32.c,
#      printwindowsmemory.c, leaklocks.c, showcrashreports.c
#                         STRACE/crash-report debug helpers; several crash
#                         the FilPizlonator (constant-relocation assertion)
#      libc/runtime/cosmo2.c, enable_tls.c
#                         aarch64 boot / yolo kernel-TLS boot (yolo side has
#                         its own; Fil-C TLS needs no kernel setup)
#      libc/intrin/tprecode8to16.c, printwindowsmemory.c
#                         pull the vendored aarch64 NEON headers
#      libc/proc/fork-nt.c vfork-nt.c msync-nt.c posix_madvise-nt.c
#                         Windows-only wrappers
#      libc/thread/pthread_cancel.c   raw asm + SIGTHR machinery; stub in filc_stub.c
#      libc/intrin/x86.c   defines __cpu_model/__cpu_indicator_init/__cpu_features2,
#                         which libpizlo.a already provides for pizlonated code
#                         (filc_native___cpu_indicator_init); including both
#                         makes every __builtin_cpu_supports() user hit a
#                         multiple-definition link error
#      libc/str/qsort.c    cosmo's introsort does unchecked pointer arithmetic
#                         on the array bounds that the filc runtime rejects on
#                         ordinary inputs; filc_extra.c provides a plain
#                         (correct, if unglamorous) qsort/qsort_r instead
#      libc/calls/madvise.c only knows the five original MADV_* advices and
#                         rejects (EINVAL) everything else before the call
#                         reaches the runtime; filc_mmap.c's madvise() forwards
#                         everything to zsys_madvise(), whose per-advice
#                         checking is what the test suite expects
#      libc/nexgen32e/envp.c duplicates the pizlonated __envp that
#                         filc_libc_start_main.c defines and fills; if both
#                         land in the link (assert() pulls envp.c.o via the
#                         crash-report helpers) every such program dies with
#                         a multiple-definition error
#      kprintf.greg.c, clone.c, seccomp.c, pledge-linux.c, islinux.c
#                         raw `syscall` inline asm without shims
#                         (islinux.c: __is_linux_2_6_23 has a C replacement in
#                         libc/calls/filc_islinux.c)
#                         NOTE: pledge.c/sysctl*.c stay in the build; their
#                         __FILC__ branches return ENOSYS instead of touching
#                         the excluded raw-asm implementations (pledge() can't
#                         be enforced under Fil-C; __syslib is never populated).
#
#    aarch64-only exclusions (AARCH64_EXCLUDE_FILES below, see the aarch64
#    branch of ARCH_CFLAGS): the metal/BIOS layer (pc.internal.h's raw I/O
#    port asm; metal is x86-only per libc/dce.h), the amd64 crash reporter,
#    the x86 cpuid table bootstrap, the EFI entrypoint, and libc/sysv/sysv.c
#    (global register variables on x0-x5, which LLVM rejects; it compiles to
#    zero symbols on x86_64 anyway).
#      mman.greg.c        bare-metal page-table code with module asm
# ─────────────────────────────────────────────────────────────────────────────
EXCLUDE_DIRS = libc/testbed libc/irq libc/dsp libc/vga libc/x8664

EXCLUDE_FILES = \
	libc/intrin/mman.greg.c \
	libc/intrin/x86.c \
	libc/str/qsort.c \
	libc/calls/madvise.c \
	libc/mem/aligned_alloc.c \
	libc/mem/posix_memalign.c \
	libc/mem/malloc_usable_size.c \
	libc/mem/leaks.c \
	libc/mem/mallopt.c \
	libc/mem/mallinfo.c \
	libc/mem/malloc_trim.c \
	libc/mem/malloc_usable_size.c \
	libc/mem/memalign.c \
	libc/mem/free.c \
	libc/mem/realloc_in_place.c \
	libc/mem/realloc.c \
	libc/mem/calloc.c \
	libc/mem/malloc.c \
	third_party/dlmalloc/dlmalloc_abort.c \
	third_party/dlmalloc/dlmalloc.c \
	libc/str/wcsstr.c \
	libc/str/strrchr16.c \
	libc/str/strchrnul16.c \
	libc/str/rawmemchr32.c \
	libc/intrin/strlen16.c \
	libc/str/memcasecmp.c \
	libc/str/memrchr16.c \
	libc/str/strstr16.c \
	libc/str/rawmemchr16.c \
	libc/str/memchr16.c \
	libc/str/strncmp16.c \
	libc/str/strnlen16.c \
	libc/str/wcscpy.c \
	libc/str/wcscmp.c \
	libc/str/wcsrchr.c \
	libc/str/wcschr.c \
	libc/str/wcslen.c \
	libc/str/strncat16.c \
	libc/str/strcat16.c \
	libc/str/strcpy16.c \
	libc/str/strcmp16.c \
	libc/str/strchr16.c \
	libc/str/strlen16.c \
	libc/str/memmem.c \
	libc/str/strcasestr.c \
	libc/str/strstr.c \
	libc/str/strncpy.c \
	libc/str/strncat.c \
	libc/str/strcat.c \
	libc/intrin/memchr.c \
	libc/intrin/strcpy.c \
	libc/intrin/strncmp.c \
	libc/intrin/strcmp.c \
	libc/intrin/strchrnul.c \
	libc/intrin/strrchr.c \
	libc/intrin/strchr.c \
	libc/intrin/strnlen.c \
	libc/intrin/strlen.c \
	libc/intrin/kprintf.greg.c \
	libc/intrin/exit1.greg.c \
	libc/intrin/mmap.c \
	libc/intrin/munmap.c \
	libc/intrin/mprotect.c \
	libc/intrin/msync.c \
	libc/intrin/madvise.c \
	libc/intrin/mincore.c \
	libc/intrin/mremap.c \
	libc/intrin/mlock.c \
	libc/intrin/munlock.c \
	libc/intrin/mlockall.c \
	libc/intrin/munlockall.c \
	libc/intrin/msync.c \
	libc/intrin/maps.c \
	libc/intrin/stack.c \
	libc/intrin/permalloc.c \
	libc/intrin/describefds.c \
	libc/intrin/brain16.c \
	libc/intrin/float16.c \
	libc/intrin/leaklocks.c \
	libc/intrin/demangle.c \
	libc/intrin/tprecode8to16.c \
	libc/intrin/printmapswin32.c \
	libc/intrin/describentpageflags.c \
	libc/intrin/describemremapflags.c \
	libc/intrin/describeallocationtype.c \
	libc/intrin/describecontrolkeystate.c \
	libc/intrin/describednotify.c \
	libc/intrin/describecapability.c \
	libc/intrin/describethreadcreationflags.c \
	libc/intrin/describepersonalityflags.c \
	libc/intrin/describentfilemapflags.c \
	libc/intrin/describentfileflagattr.c \
	libc/intrin/describentsymlinkflags.c \
	libc/intrin/describentstartflags.c \
	libc/intrin/describentpipemodeflags.c \
	libc/intrin/describentprocaccessflags.c \
	libc/intrin/describentpipeopenflags.c \
	libc/intrin/describentmovfileinpflags.c \
	libc/intrin/describentlockfileflags.c \
	libc/intrin/describentfileshareflags.c \
	libc/intrin/describentfiletypeflags.c \
	libc/intrin/describentfileaccessflags.c \
	libc/intrin/describentconsolemodeoutputflags.c \
	libc/intrin/describentconsolemodeinputflags.c \
	libc/runtime/clone.c \
	libc/runtime/cosmo2.c \
	libc/runtime/enable_tls.c \
	libc/runtime/utmp.c \
	libc/nexgen32e/environ2.c \
	libc/nexgen32e/auxv2.c \
	libc/runtime/fork-nt.c \
	libc/calls/pledge-linux.c \
	libc/calls/seccomp.c \
	libc/calls/islinux.c \
	libc/calls/sigaction.c \
	libc/runtime/zipos-stat.c \
	libc/runtime/zipos-stat-impl.c \
	libc/runtime/zipos-seek.c \
	libc/runtime/zipos-read.c \
	libc/runtime/zipos-parseuri.c \
	libc/runtime/zipos-open.c \
	libc/runtime/zipos-notat.c \
	libc/runtime/zipos-normpath.c \
	libc/runtime/zipos-mmap.c \
	libc/runtime/zipos-inode.c \
	libc/runtime/zipos-get.c \
	libc/runtime/zipos-fstat.c \
	libc/runtime/zipos-find.c \
	libc/runtime/zipos-close.c \
	libc/runtime/zipos-access.c \
	libc/thread/pthread_cancel.c \
	libc/thread/makecontext.c \
	libc/thread/pthread_getaffinity_np.c \
	libc/thread/pthread_setaffinity_np.c \
	libc/proc/fork-nt.c \
	libc/proc/vfork-nt.c \
	libc/nexgen32e/envp.c \
	libc/log/printwindowsmemory.c \
	libc/intrin/posix_madvise-nt.c \
	libc/intrin/msync-nt.c \
	libc/dlopen/dlopen.c \
	libc/dlopen/dlclose.c \
	libc/dlopen/dlsym.c

AARCH64_EXCLUDE_FILES ?=

# third_party trees that are part of the v1 library
THIRD_PARTY_DIRS = third_party/nsync third_party/gdtoa third_party/tz

# core libc
SRCS_C = $(shell cd $(COSMO) && find libc -name '*.c' | LC_ALL=C sort)
SRCS_C := $(filter-out $(addsuffix /%,$(EXCLUDE_DIRS)),$(SRCS_C))
SRCS_C := $(filter-out $(EXCLUDE_FILES) $(AARCH64_EXCLUDE_FILES),$(SRCS_C))
# tool directories that require the cosmo build machinery / yolo-only
SRCS_C := $(filter-out libc/sysv/calls/% libc/crt/%,$(SRCS_C))
# third_party
SRCS_C += $(foreach d,$(THIRD_PARTY_DIRS),$(shell cd $(COSMO) && find $(d) -name '*.c' | LC_ALL=C sort))
# third_party/getopt: the __optarg/__optind/__getopt implementation that the
# sed/tr applets use; third_party/regex: POSIX regcomp/regexec used by sed
# (and by glob-style tools); musl's pwd.c backs getpwnam_r/getpwuid_r which
# glob.c calls.
SRCS_C += $(shell cd $(COSMO) && find third_party/getopt third_party/regex -name '*.c' | LC_ALL=C sort)
SRCS_C += $(foreach m,pwd.c fgetspent.c getspnam_r.c putspent.c,\
	$(shell cd $(COSMO) && find third_party/musl -name '$(m)' 2>/dev/null))
# selected musl bits.  cosmo vendors chunks of musl in third_party/musl/ and
# uses them as the canonical implementations of the corresponding APIs; the
# files below are the pieces the test suite needs (search tree, netdb/
# resolver, locale, wctype, glob), plus the previously present strftime/
# langinfo group.  Everything here is plain C that compiles under Fil-C.
MUSL_FILES = strftime*.c wcsftime*.c timelocal*.c langinfo.c asctime*.c \
	__tm_to_secs.c lctrans.c __mo_lookup.c locinfo.c __month_to_secs.c \
	__year_to_secs.c __secs_to_tm.c __days_from_civil.c \
	tsearch.c tfind.c tdelete.c tdestroy.c twalk.c \
	getaddrinfo.c freeaddrinfo.c gai_strerror.c getnameinfo.c \
	getservbyname.c getservbyname_r.c getservbyport.c getservbyport_r.c \
	gethostbyname.c gethostbyname_r.c gethostbyname2.c gethostbyname2_r.c \
	gethostbyaddr.c gethostbyaddr_r.c h_errno.c herror.c hstrerror.c \
	lookup_name.c lookup_ipliteral.c lookup_serv.c \
	res_mkquery.c res_msend.c res_send.c res_query.c res_querydomain.c \
	res_state.c res_init.c resolvconf.c dns_parse.c dn_comp.c dn_expand.c \
	dn_skipname.c proto.c serv.c \
	setlocale.c locale_map.c newlocale.c duplocale.c freelocale.c uselocale.c \
	iswctype.c iswalnum.c iswalpha.c iswpunct.c wctrans.c towctrans.c \
	catopen.c catgets.c catclose.c \
	btowc.c wctob.c \
	mbrtowc.c wcrtomb.c mbsinit.c mbstowcs.c wcstombs.c mbsrtowcs.c \
	mbsnrtowcs.c wcsnrtombs.c wcsrtombs.c mbrlen.c mblen.c mbtowc.c \
	wctomb.c mbrtoc16.c mbrtoc32.c c16rtomb.c c32rtomb.c multibyte.c \
	mapfile.c \
	glob.c fnmatch.c
SRCS_C += $(foreach m,$(MUSL_FILES),\
	$(shell cd $(COSMO) && find third_party/musl -name '$(m)' 2>/dev/null))
# The embedded sed/tr applets that cosmo's system()/popen() shell (cocmd)
# dispatches to; without them every system() user has undefined symbols.
SRCS_C += $(shell cd $(COSMO) && find third_party/sed third_party/tr -name '*.c' | LC_ALL=C sort)

# Every source becomes o-filc/<path>.o (and the generated ones become
# o-filc/generated/<name>.o), exactly like the old build_usercosmo.sh.
OBJS = $(addprefix $(BUILD)/,$(addsuffix .o,$(SRCS_C)))
CCXX_OBJS = $(addprefix $(BUILD)/,$(addsuffix .o,$(CCXX)))

.PHONY: print-sources
print-sources:
	@echo "$(words $(SRCS_C)) C sources + $(words $(CCXX)) C++ sources"

# ─────────────────────────────────────────────────────────────────────────────
# 4. Compile rules.  Incremental: an object is recompiled when its source is
#    newer (make's usual mtime comparison), which is exactly what the old
#    build_usercosmo.sh's compile.sh did.  Header-only changes need FORCE=1
#    since headers are not tracked.  Compiler stderr is appended to
#    o-filc/compile-errors.log, like before; a failure aborts the build
#    (the old script soldiered on and archived whatever compiled - with make
#    we stop instead of shipping a half-built libc.a).
# ─────────────────────────────────────────────────────────────────────────────
$(BUILD)/%.c.o: %.c $(FORCE_PREREQ) | check-env
	@mkdir -p $(dir $@)
	@$(FILC_CLANG) $(CFLAGS) -c -o $@ $< \
	    2>>$(BUILD)/compile-errors.log || { \
	    echo "FAILED: $< (details in $(FILC_BUILDDIR)/compile-errors.log)" >&2; \
	    echo "FAILED: $<" >> $(BUILD)/compile-failures.txt; \
	    exit 1; }

$(BUILD)/%.cc.o: %.cc $(FORCE_PREREQ) | check-env
	@mkdir -p $(dir $@)
	@$(FILC_CLANG) $(CXXFLAGS) -c -o $@ $< \
	    2>>$(BUILD)/compile-errors.log || { \
	    echo "FAILED: $< (details in $(FILC_BUILDDIR)/compile-errors.log)" >&2; \
	    echo "FAILED: $<" >> $(BUILD)/compile-failures.txt; \
	    exit 1; }

# ─────────────────────────────────────────────────────────────────────────────
# 5. Archive everything into pizfix/lib/libc.a (the cosmo flavor's -lc).
#    Member names are the object paths relative to o-filc, and the list is
#    sorted, exactly like the old build_usercosmo.sh.
#
#    One deliberate difference: the old script archived `find o-filc -name
#    '*.o'`, so re-runs would sweep stale artifacts (in particular crt1.o's
#    fused inputs from the previous run) into libc.a.  Those members could
#    never be pulled by the linker - their symbols are already defined by the
#    explicit pizfix/lib/crt1.o - so archiving exactly the objects that this
#    makefile builds is equivalent, just deterministic.
# ─────────────────────────────────────────────────────────────────────────────
LIBC_A_OBJS = $(sort $(patsubst $(BUILD)/%,%,$(OBJS) $(CCXX_OBJS) $(GEN_OBJS)))

$(LIBDIR)/libc.a: $(OBJS) $(CCXX_OBJS) $(GEN_OBJS) | check-env
	@mkdir -p $(LIBDIR)
	@rm -f $@
	cd $(BUILD) && ar crs $(LIBDIR)/libc.a $(LIBC_A_OBJS)

# ─────────────────────────────────────────────────────────────────────────────
# 5b. libm.a: cosmo folds all of libm into libc.a, but portable programs (and
#     the test suite) link with -lm.  musl's flavor ships a real libm.a; give
#     the cosmo flavor an empty one so that -lm resolves.
# ─────────────────────────────────────────────────────────────────────────────
$(LIBDIR)/libm.a: $(LIBDIR)/libc.a
	@rm -f $@
	ar crs $@

# ─────────────────────────────────────────────────────────────────────────────
# 6. crt1.o: the cosmo yolo crt (raw _start → cosmo() boot, installed by
#    build_yolocosmo.sh as cosmo-crt.o) fused with yolo glue.  The glue
#    provides plain (un-pizlonated) symbols that libpizlo.a's yolo side
#    references but that libyolocosmo doesn't define (llrintl).  It has to be
#    fused into crt1.o because the link order is crt1.o … -lc … -lpizlo, and
#    an archive member of libc.a is only pulled when its symbols are already
#    referenced — which for llrintl happens later, inside libpizlo.a.
#
#    NOTE: the glue must be compiled by the HOST compiler: the Fil-C clang
#    would pizlonate it into pizlonated_* symbols, and the whole point is to
#    provide plain ones for the yolo side.
# ─────────────────────────────────────────────────────────────────────────────
# The glue is plain (un-pizlonated) aarch64/x86_64 code, so it is compiled by
# the plain host clang, with --target for cross builds.  The -r fusion of
# yolo objects needs a linker with an emulation for the target architecture:
# the host GNU ld is x86_64-only, so cross builds use the aarch64 GNU ld that
# ships in cosmo's .cosmocc toolchain (the same one that links the yolo libc).
YOLOGLUECC = clang $(if $(filter aarch64,$(FILCARCH)),--target=aarch64-linux-gnu,)
AARCH64_LD := $(firstword $(wildcard $(ROOT)/projects/yolocosmo/.cosmocc/*/bin/aarch64-linux-cosmo-ld))
LD_FUSE = $(if $(filter aarch64,$(FILCARCH)),$(AARCH64_LD),ld)

$(BUILD)/yologlue.o: $(COSMO)/filc/yologlue.c $(FORCE_PREREQ) | check-env
	@mkdir -p $(dir $@)
	$(YOLOGLUECC) -c -o $@ $(COSMO)/filc/yologlue.c \
	    -nostdinc -isystem $(PIZFIX)/yolo-include \
	    -include $(PIZFIX)/yolo-include/normalize.inc -D__COSMOPOLITAN__ \
	    -w

$(BUILD)/crt1.fused.o: $(LIBDIR)/cosmo-crt.o $(BUILD)/yologlue.o | check-env
	$(LD_FUSE) -r -o $@ $(LIBDIR)/cosmo-crt.o $(BUILD)/yologlue.o

$(LIBDIR)/crt1.o: $(BUILD)/crt1.fused.o | check-env
	cp -f $< $@

# ─────────────────────────────────────────────────────────────────────────────
# 7. Install the cosmo public headers into pizfix/include.  pizfix/include
#    gets only the headers of code that is actually part of this libc:
#
#    a. cosmo's isystem public headers, minus the wrappers whose only job is
#       to forward to cosmo's vendored C++-runtime/openmp/libunwind trees
#       (cxxabi.h, omp.h, omp-tools.h, ompx.h, unwind.h, experimental/, ext/,
#       module.modulemap, libcxx.imp).  None of those vendored trees are
#       built in the Fil-C world: C++ code uses the LLVM libc++/libc++abi
#       that the driver searches ahead of pizfix/include (<cxxabi.h> resolves
#       to build/include/c++/v1/cxxabi.h), and <unwind.h> resolves to Fil-C's
#       own pizfix/stdfil-include/unwind.h for both C and C++.  <omp.h> would
#       be declarations without a library anyway (the Fil-C usercosmo build
#       never compiles third_party/openmp, so there is no libomp to link
#       against), so shipping it is a link-time trap.  The five .h wrappers
#       are excluded from the internal-tree copy below as well: the find
#       over libc/ would otherwise reinstall them, dangling, at the nested
#       pizfix/include/libc/isystem/ path (the extensionless experimental/
#       and ext/ wrappers and module.modulemap/libcxx.imp need no such
#       exclusion there, since they match neither *.h nor *.inc).
#
#    b. internal headers only for code that is actually compiled into this
#       libc: libc/**, the third_party dirs that are archived into libc.a
#       (gdtoa, getopt, musl, nsync, regex, sed, tr, tz), the header-only
#       compiler intrinsic trees third_party/intel and third_party/aarch64
#       (what <immintrin.h> and <arm_neon.h> resolve to), and the two
#       internal trees that installed public headers cross-include
#       (dsp/audio/cosmoaudio via <cosmoaudio.h> and net/http via <cosmo.h>).
#       The rest of third_party (zlib, lua, python, mbedtls, sqlite3, ...)
#       and ape/ and ctl/ are NOT installed: cosmo is *just* the libc in the
#       Fil-C world; user libraries are built the Fil-C way, not vendored
#       from cosmo's tree.
#
#    cosmo's isystem headers assume that normalize.inc was force-included
#    first (cosmocc does `-include libc/integral/normalize.inc`), which is
#    what provides COSMOPOLITAN_C_START_/bool32/libcesque/etc.  The Fil-C
#    driver has no equivalent of that wrapper flag, so every installed
#    isystem header gets the normalize include prepended.  normalize.inc is
#    idempotent (c.inc and friends have proper include guards), so multiple
#    inclusions are harmless.
# ─────────────────────────────────────────────────────────────────────────────
.PHONY: install-headers
install-headers: | check-env
	rm -rf $(PIZFIX)/include
	mkdir -p $(PIZFIX)/include
	(cd $(COSMO)/libc/isystem && find . -type f) | LC_ALL=C sort > $(BUILD)/headers.txt
	(cd $(COSMO)/libc/isystem && tar -c \
	    --exclude=./cxxabi.h --exclude=./omp.h --exclude=./omp-tools.h \
	    --exclude=./ompx.h --exclude=./unwind.h --exclude=./experimental \
	    --exclude=./ext --exclude=./module.modulemap --exclude=./libcxx.imp \
	    -f - .) | (cd $(PIZFIX)/include && tar -xf -)
	(cd $(COSMO) && find libc dsp/audio/cosmoaudio net/http \
	    third_party/aarch64 third_party/gdtoa third_party/getopt \
	    third_party/intel third_party/musl third_party/nsync \
	    third_party/regex third_party/sed third_party/tr third_party/tz \
	    \( -name '*.h' -o -name '*.inc' \) \
	    ! -path libc/isystem/cxxabi.h ! -path libc/isystem/omp.h \
	    ! -path libc/isystem/omp-tools.h ! -path libc/isystem/ompx.h \
	    ! -path libc/isystem/unwind.h \
	    | tar -cf - -T -) | (cd $(PIZFIX)/include && tar -xf -)
	cp -f $(COSMO)/libc/integral/*.inc $(PIZFIX)/include/ 2>/dev/null || true
	while read -r h; do \
	    h="$${h#./}"; \
	    target="$(PIZFIX)/include/$$h"; \
	    [ -f "$$target" ] || continue; \
	    if ! grep -q "libc/integral/normalize.inc" "$$target"; then \
		printf '#ifdef __FILC__\n/* Fil-C port: force-include normalize.inc (see build_usercosmo.sh). */\n/* Also default to _GNU_SOURCE: the test suite and portable Linux code\n * expect the POSIX+BSD+GNU declaration surface that musl (whose headers\n * are the musl flavor'"'"'s default) exposes without explicit feature-test\n * macros.  cosmo hides those declarations unless a feature macro is set,\n * which would turn a large class of otherwise-fine programs into compile\n * errors for no good reason. */\n#ifndef _GNU_SOURCE\n#define _GNU_SOURCE 1\n#endif\n#include "libc/integral/normalize.inc"\n#endif\n' > "$$target.new"; \
		cat "$$target" >> "$$target.new"; \
		mv "$$target.new" "$$target"; \
	    fi; \
	done < $(BUILD)/headers.txt

# ─────────────────────────────────────────────────────────────────────────────
# 8. Preconditions.  These run before anything else gets built; they are the
#    same checks (and the same error messages) that build_usercosmo.sh
#    performed before doing any work.
# ─────────────────────────────────────────────────────────────────────────────
.PHONY: check-env
check-env:
	@test -x "$(FILC_CLANG)" || { \
	    echo "error: $(FILC_CLANG) not found; build the Fil-C compiler first" >&2; exit 1; }
	@test -e "$(LIBDIR)/libyolocosmo.a" || { \
	    echo "error: $(LIBDIR)/libyolocosmo.a missing; run ./build_yolocosmo.sh first" >&2; exit 1; }
	@test -e "$(LIBDIR)/libpizlo.a" -a ! -e "$(LIBDIR)/libpizlo.so" || { \
	    echo "error: $(LIBDIR)/libpizlo.a is not the cosmo-mode build;" >&2; \
	    echo "       run ./build_yolocosmo.sh && (cd libpas && make FILCARCH=$(FILCARCH) ...)" >&2; \
	    exit 1; }
	@mkdir -p $(BUILD)
	@rm -f $(BUILD)/compile-failures.txt
	@: > $(BUILD)/compile-errors.log

ifneq ($(FILCARCH),$(shell uname -m))
# A cross build shares pizfix/include with the host build (cosmo's headers
# are arch-neutral, see build_usercosmo.sh), so the headers only get
# (re)installed by the host-arch build.
INSTALL_TARGETS = $(LIBDIR)/libc.a $(LIBDIR)/libm.a $(LIBDIR)/crt1.o
else
INSTALL_TARGETS = $(LIBDIR)/libc.a $(LIBDIR)/libm.a $(LIBDIR)/crt1.o install-headers
endif

all install: check-env $(INSTALL_TARGETS)
