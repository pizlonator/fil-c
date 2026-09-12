# Copyright (C) 1991-2026 Free Software Foundation, Inc.
# This file is part of the GNU C Library.

# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.

# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, see
# <https://www.gnu.org/licenses/>.

#
#	Makefile for malloc routines
#
subdir	:= malloc

include ../Makeconfig

dist-headers := malloc.h
headers := $(dist-headers) obstack.h mcheck.h
tests := \
  mallocbug \
  tst-aligned-alloc \
  tst-aligned-alloc-random \
  tst-aligned-alloc-random-thread \
  tst-aligned-alloc-random-thread-cross \
  tst-alloc_buffer \
  tst-calloc \
  tst-free-aligned-sized \
  tst-free-aligned-sized-trace \
  tst-free-errno \
  tst-free-sized \
  tst-free-sized-trace \
  tst-interpose-nothread \
  tst-interpose-thread \
  tst-mallinfo2 \
  tst-malloc \
  tst-malloc-alternate-path \
  tst-malloc-backtrace \
  tst-malloc-check \
  tst-malloc-fork-deadlock \
  tst-malloc-random \
  tst-malloc-stats-cancellation \
  tst-malloc-tcache-leak \
  tst-malloc-thread-exit \
  tst-malloc-thread-fail \
  tst-malloc-too-large \
  tst-malloc-usable \
  tst-malloc_info \
  tst-mallocalign1 \
  tst-mallocfork \
  tst-mallocfork2 \
  tst-mallocfork3 \
  tst-mallopt \
  tst-memalign \
  tst-memalign-2 \
  tst-memalign-3 \
  tst-obstack \
  tst-posix_memalign \
  tst-pvalloc \
  tst-pvalloc-fortify \
  tst-realloc \
  tst-reallocarray \
  tst-safe-linking \
  tst-tcfree1 \
  tst-tcfree2 \
  tst-tcfree3 \
  tst-tcfree4 \
  tst-trim1 \
  tst-valloc \
# tests

tests-static := \
  tst-aligned-alloc-static \
  tst-interpose-static-nothread \
  tst-interpose-static-thread \
# tests-static

# Test for the malloc_set_state symbol removed in glibc 2.25.
ifeq ($(have-GLIBC_2.23)$(build-shared),yesyes)
tests += \
  tst-compathooks-off \
  tst-compathooks-on \
  tst-mallocstate \
# tests
endif

tests-internal := tst-scratch_buffer

# The dynarray framework is only available inside glibc.
tests-internal += \
  tst-dynarray \
  tst-dynarray-at-fail \
  tst-dynarray-fail \
# tests-internal

tests += \
  tst-malloc-usable-tunables \
# tests

tests += $(tests-static)
test-srcs = tst-mtrace

# These tests either are run with MALLOC_CHECK_=3 by default or do not work
# with MALLOC_CHECK_=3 because they expect a specific failure.
tests-exclude-malloc-check = \
  tst-compathooks-off \
  tst-compathooks-on \
  tst-malloc-check \
  tst-malloc-tcache-leak \
  tst-malloc-usable \
  tst-mallocfork2 \
  tst-mallocfork3 \
  tst-memalign-2 \
  tst-memalign-3 \
  tst-safe-linking \
  tst-tcfree4 \
# tests-exclude-malloc-check

# Run all tests with MALLOC_CHECK_=3
tests-malloc-check = $(filter-out $(tests-exclude-malloc-check) \
				  $(tests-static),$(tests))

# Run all tests with GLIBC_TUNABLES=glibc.malloc.hugetlb={1,2} which check
# the Transparent Huge Pages support (1) or automatic huge page support (2).
# We need exclude some tests that define the ENV vars.
tests-exclude-hugetlb1 = \
  tst-compathooks-off \
  tst-compathooks-on \
  tst-interpose-nothread \
  tst-interpose-static-nothread \
  tst-interpose-static-thread \
  tst-interpose-thread \
  tst-malloc-tcache-leak \
  tst-malloc-usable \
  tst-malloc-usable-tunables \
  tst-mallocfork2 \
  tst-mallocfork3 \
  tst-mallocstate \
# tests-exclude-hugetlb1
# The tst-free-errno relies on the used malloc page size to mmap an
# overlapping region.
tests-exclude-hugetlb2 = \
	$(tests-exclude-hugetlb1) \
	tst-free-errno
tests-malloc-hugetlb1 = \
	$(filter-out $(tests-exclude-hugetlb1), $(tests))
tests-malloc-hugetlb2 = \
	$(filter-out $(tests-exclude-hugetlb2), $(tests))

tests-exclude-largetcache = \
  tst-compathooks-off \
  tst-compathooks-on \
  tst-interpose-nothread \
  tst-interpose-static-nothread \
  tst-interpose-static-thread \
  tst-interpose-thread \
  tst-malloc-backtrace \
  tst-malloc-usable \
  tst-malloc-usable-tunables \
  tst-mallocstate \
# tests-exclude-largetcache

tests-malloc-largetcache = \
	$(filter-out $(tests-exclude-largetcache), $(tests))

# -lmcheck needs __malloc_initialize_hook, which was deprecated in 2.24.
ifeq ($(have-GLIBC_2.23)$(build-shared),yesyes)
# Tests that don't play well with mcheck.  They are either bugs in mcheck or
# the tests expect specific internal behavior that is changed due to linking to
# libmcheck.a.
tests-exclude-mcheck = \
  tst-aligned-alloc-random-thread \
  tst-aligned-alloc-random-thread-cross \
  tst-compathooks-off \
  tst-compathooks-on \
  tst-malloc-backtrace \
  tst-malloc-fork-deadlock \
  tst-malloc-stats-cancellation \
  tst-malloc-tcache-leak \
  tst-malloc-thread-exit \
  tst-malloc-thread-fail \
  tst-malloc-usable-tunables \
  tst-malloc_info \
  tst-mallocfork2 \
  tst-mallocfork3 \
  tst-mallocstate \
  tst-memalign-2 \
  tst-memalign-3 \
  tst-safe-linking \
# tests-exclude-mcheck

tests-mcheck = $(filter-out $(tests-exclude-mcheck) $(tests-static), $(tests))
endif

routines = malloc mcheck mtrace obstack reallocarray \
  scratch_buffer_grow scratch_buffer_grow_preserve \
  scratch_buffer_set_array_size \
  dynarray_at_failure \
  dynarray_emplace_enlarge \
  dynarray_finalize \
  dynarray_resize \
  dynarray_resize_clear \
  alloc_buffer_alloc_array \
  alloc_buffer_allocate \
  alloc_buffer_copy_bytes  \
  alloc_buffer_copy_string \
  alloc_buffer_create_failure \

install-lib := libmcheck.a
non-lib.a := libmcheck.a

# Additional libraries.
extra-libs = libmemusage libc_malloc_debug
extra-libs-others = $(extra-libs)

# Helper objects for some tests.
extra-test-objs += \
  tst-aligned_alloc-lib.so \
  tst-interpose-aux-nothread.o \
  tst-interpose-aux-thread.o \
# extra-test-objs

test-extras = \
  tst-interpose-aux-nothread \
  tst-interpose-aux-thread \
# test-extras

modules-names = \
  tst-aligned_alloc-lib

libmemusage-routines = memusage
libmemusage-inhibit-o = $(filter-out .os,$(object-suffixes))

libc_malloc_debug-routines = malloc-debug $(sysdep_malloc_debug_routines)
libc_malloc_debug-inhibit-o = $(filter-out .os,$(object-suffixes))

tests-link-with-libpthread = \
  tst-aligned-alloc-random-thread \
  tst-aligned-alloc-random-thread-cross \
  tst-aligned-alloc-random-thread-cross-malloc-check \
  tst-aligned-alloc-random-thread-cross-malloc-hugetlb1 \
  tst-aligned-alloc-random-thread-cross-malloc-hugetlb2 \
  tst-aligned-alloc-random-thread-malloc-check \
  tst-aligned-alloc-random-thread-malloc-hugetlb1 \
  tst-aligned-alloc-random-thread-malloc-hugetlb2 \
  tst-interpose-thread \
  tst-interpose-thread-malloc-check \
  tst-interpose-thread-mcheck \
  tst-malloc-backtrace \
  tst-malloc-backtrace-malloc-check \
  tst-malloc-backtrace-mcheck \
  tst-malloc-fork-deadlock \
  tst-malloc-fork-deadlock-malloc-check \
  tst-malloc-fork-deadlock-malloc-hugetlb1 \
  tst-malloc-fork-deadlock-malloc-hugetlb2 \
  tst-malloc-fork-deadlock-mcheck \
  tst-malloc-stats-cancellation \
  tst-malloc-stats-cancellation-malloc-check \
  tst-malloc-stats-cancellation-malloc-hugetlb1 \
  tst-malloc-stats-cancellation-malloc-hugetlb2 \
  tst-malloc-stats-cancellation-mcheck \
  tst-malloc-tcache-leak \
  tst-malloc-thread-exit \
  tst-malloc-thread-exit-malloc-check \
  tst-malloc-thread-exit-malloc-hugetlb1 \
  tst-malloc-thread-exit-malloc-hugetlb2 \
  tst-malloc-thread-exit-mcheck \
  tst-malloc-thread-fail \
  tst-malloc-thread-fail-malloc-check \
  tst-malloc-thread-fail-malloc-hugetlb1 \
  tst-malloc-thread-fail-malloc-hugetlb2 \
  tst-malloc-thread-fail-mcheck \
  tst-malloc_info \
  tst-malloc_info-malloc-check \
  tst-malloc_info-malloc-hugetlb1 \
  tst-malloc_info-malloc-hugetlb2 \
  tst-malloc_info-mcheck \
  tst-mallocfork2 \
  tst-mallocfork2-malloc-check \
  tst-mallocfork3 \
  tst-mallocfork3-malloc-check \
  tst-mallocfork3-mcheck \
  tst-memalign-3 \
  tst-memalign-3-malloc-hugetlb1 \
  tst-memalign-3-malloc-hugetlb2 \
# tests-link-with-libpthread

$(addprefix $(objpfx), $(tests-link-with-libpthread)): $(shared-thread-library)
# Not all these tests are actually built, see tests-exclude-largetcache.
$(tests-link-with-libpthread:%=$(objpfx)%-malloc-largetcache): \
  $(shared-thread-library)

# tests-threaded-{main,worker} are variants of base malloc tests that are
# run in a multi-threaded program.  The main variant is run in the main
# thread while an alternate thread exists, and the worker variant is run in
# a worker thread while the main thread waits on it.

# List of tests which don't have threaded-{main,worker} copies:
# already multithreaded tests,
# static copies of some regular tests,
# tst-compathooks-on: hook/call counts mismatch due to threading allocations
# tst-interpose-nothread: interposes malloc without locking
# tst-mallocfork: calls fork from a signal handler
#   (tst-mallocfork{2,3} already excluded via tests-link-with-libpthread)
tests-exclude-threaded = \
  $(tests-link-with-libpthread) \
  $(tests-static) \
  tst-compathooks-on \
  tst-interpose-nothread \
  tst-mallocfork \
# tests-exclude-threaded

tests-threaded-main = $(addsuffix -threaded-main, \
				  $(filter-out $(tests-exclude-threaded), \
					       $(tests)))
$(addprefix $(objpfx), $(tests-threaded-main)): $(shared-thread-library)

tests-threaded-worker = $(addsuffix -threaded-worker, \
				    $(filter-out $(tests-exclude-threaded), \
						 $(tests)))
$(addprefix $(objpfx), $(tests-threaded-worker)): $(shared-thread-library)

tests += $(tests-threaded-main) $(tests-threaded-worker)

# This avoids a test combination explosion with two threaded copies of each
# of these variants:
tests-exclude-malloc-check += $(tests-threaded-main) $(tests-threaded-worker)
tests-exclude-mcheck += $(tests-threaded-main) $(tests-threaded-worker)
tests-exclude-hugetlb1 += $(tests-threaded-main) $(tests-threaded-worker)
tests-exclude-hugetlb2 += $(tests-threaded-main) $(tests-threaded-worker)
tests-exclude-largetcache += $(tests-threaded-main) $(tests-threaded-worker)

# These should be removed by `make clean'.
extra-objs = mcheck-init.o libmcheck.a
others-extras = mcheck-init.o

# Compile threaded-{main,worker} tests from their corresponding base
# sources, but with -DTEST_IN_THREAD appropriately defined so that support
# infrastructure runs them in a multi-threaded program.
$(objpfx)%-threaded-main.o: CPPFLAGS += -DTEST_IN_THREAD=TEST_THREAD_MAIN
$(objpfx)%-threaded-main.o: %.c $(before-compile)
	$(compile-command.c)
$(objpfx)%-threaded-worker.o: CPPFLAGS += -DTEST_IN_THREAD=TEST_THREAD_WORKER
$(objpfx)%-threaded-worker.o: %.c $(before-compile)
	$(compile-command.c)

# Compile the hugetlb={1,2} test variants from their base sources with
# -DTEST_HUGETLB so support/test-driver.c gates them on kernel support.
$(foreach t,$(tests-malloc-hugetlb1),\
  $(eval libof-$(t)-malloc-hugetlb1 := testsuite))
$(foreach t,$(tests-malloc-hugetlb2),\
  $(eval libof-$(t)-malloc-hugetlb2 := testsuite))
$(objpfx)%-malloc-hugetlb1.o: CPPFLAGS += -DTEST_HUGETLB
$(objpfx)%-malloc-hugetlb1.o: %.c $(before-compile)
	$(compile-command.c)
$(objpfx)%-malloc-hugetlb2.o: CPPFLAGS += -DTEST_HUGETLB
$(objpfx)%-malloc-hugetlb2.o: %.c $(before-compile)
	$(compile-command.c)

# Include the cleanup handler.
aux := set-freeres thread-freeres

# The Perl script to analyze the output of the mtrace functions.
install-bin-script = mtrace
generated += mtrace

# The Perl script will print addresses and to do this nicely we must know
# whether we are on a 32 or 64 bit machine.
ifneq ($(findstring wordsize-32,$(config-sysdirs)),)
address-width=10
else
address-width=18
endif

# Unless we get a test for the availability of libgd which also works
# for cross-compiling we disable the memusagestat generation in this
# situation.
ifneq ($(cross-compiling),yes)
# If the gd library is available we build the `memusagestat' program.
ifneq ($(LIBGD),no)
others: $(objpfx)memusage
others += memusagestat
install-bin = memusagestat
install-bin-script += memusage
generated += memusagestat memusage
extra-objs += memusagestat.o

# The configure.ac check for libgd and its headers did not use $SYSINCLUDES.
# The directory specified by --with-headers usually contains only the basic
# kernel interface headers, not something like libgd.  So the simplest thing
# is to presume that the standard system headers will be ok for this file.
$(objpfx)memusagestat.o: sysincludes = # nothing
endif
endif

# Another goal which can be used to override the configure decision.
.PHONY: do-memusagestat
do-memusagestat: $(objpfx)memusagestat

memusagestat-modules = memusagestat

cpp-srcs-left := $(memusagestat-modules)
lib := memusagestat
include $(patsubst %,$(..)libof-iterator.mk,$(cpp-srcs-left))

LDLIBS-memusagestat = $(libgd-LDFLAGS) -lgd -lpng -lz -lm

ifeq ($(run-built-tests),yes)
ifeq (yes,$(build-shared))
ifneq ($(PERL),no)
tests-special += $(objpfx)tst-mtrace.out
tests-special += $(objpfx)tst-dynarray-mem.out
tests-special += $(objpfx)tst-dynarray-fail-mem.out
tests-special += $(objpfx)tst-free-aligned-sized-mtrace.out
tests-special += $(objpfx)tst-free-sized-mtrace.out
endif
endif
endif

include ../Rules

CFLAGS-mcheck-init.c += $(PIC-ccflag)
CFLAGS-obstack.c += $(uses-callbacks)

$(objpfx)libmcheck.a: $(objpfx)mcheck-init.o
	-rm -f $@
	$(patsubst %/,cd % &&,$(objpfx)) \
	$(LN_S) $(<F) $(@F)

lib: $(objpfx)libmcheck.a

ifeq ($(run-built-tests),yes)
ifeq (yes,$(build-shared))
ifneq ($(PERL),no)
$(objpfx)tst-mtrace.out: tst-mtrace.sh $(objpfx)tst-mtrace
	$(SHELL) $< $(common-objpfx) '$(test-program-prefix-before-env)' \
		 '$(run-program-env)' '$(test-program-prefix-after-env)'; \
	$(evaluate-test)
endif
endif
endif

malloc-check-env = \
  MALLOC_CHECK_=3 \
  LD_PRELOAD=$(objpfx)/libc_malloc_debug.so

tst-malloc-check-ENV = $(malloc-check-env)
tst-malloc-usable-ENV = $(malloc-check-env)
tst-malloc-usable-threaded-main-ENV = $(malloc-check-env)
tst-malloc-usable-threaded-worker-ENV = $(malloc-check-env)

malloc-check-tunables-env = LD_PRELOAD=$(objpfx)/libc_malloc_debug.so
malloc-check-tunables-tun = glibc.malloc.check=3
tst-malloc-usable-tunables-ENV = $(malloc-check-tunables-env)
tst-malloc-usable-tunables-TUNABLES += $(malloc-check-tunables-tun)
tst-malloc-usable-tunables-threaded-main-ENV = $(malloc-check-tunables-env)
tst-malloc-usable-tunables-threaded-main-TUNABLES += $(malloc-check-tunables-tun)
tst-malloc-usable-tunables-threaded-worker-ENV = $(malloc-check-tunables-env)
tst-malloc-usable-tunables-threaded-worker-TUNABLES += $(malloc-check-tunables-tun)

CPPFLAGS-malloc-debug.c += -DUSE_TCACHE=0
CPPFLAGS-malloc.c += -DUSE_TCACHE=1
# Uncomment this for test releases.  For public releases it is too expensive.
#CPPFLAGS-malloc.o += -DMALLOC_DEBUG=1

CFLAGS-tst-tcfree3.c += -fno-builtin-malloc -fno-builtin-free

sLIBdir := $(shell echo $(slibdir) | sed 's,lib\(\|64\)$$,\\\\$$LIB,')

$(objpfx)mtrace: mtrace.pl
	rm -f $@.new
	sed -e 's|@XXX@|$(address-width)|' \
	    -e 's|@VERSION@|$(version)|' \
	    -e 's|@PKGVERSION@|$(PKGVERSION)|' \
	    -e 's|@REPORT_BUGS_TO@|$(REPORT_BUGS_TO)|' $^ > $@.new \
	&& rm -f $@ && mv $@.new $@ && chmod +x $@

$(objpfx)memusage: memusage.sh
	rm -f $@.new
	sed -e 's|@VERSION@|$(version)|' -e 's|@SLIBDIR@|$(sLIBdir)|' \
	    -e 's|@BINDIR@|$(bindir)|' -e 's|@PKGVERSION@|$(PKGVERSION)|' \
	    -e 's|@REPORT_BUGS_TO@|$(REPORT_BUGS_TO)|' $^ > $@.new \
	&& rm -f $@ && mv $@.new $@ && chmod +x $@

# Compile the tests with a flag which suppresses the mallopt call in
# the test skeleton.
$(tests:%=$(objpfx)%.o): CPPFLAGS += -DTEST_NO_MALLOPT

$(objpfx)tst-interpose-nothread: $(objpfx)tst-interpose-aux-nothread.o
$(objpfx)tst-interpose-nothread-mcheck: $(objpfx)tst-interpose-aux-nothread.o
$(objpfx)tst-interpose-nothread-malloc-check: \
	$(objpfx)tst-interpose-aux-nothread.o
$(objpfx)tst-interpose-thread: $(objpfx)tst-interpose-aux-thread.o
$(objpfx)tst-interpose-thread-mcheck: $(objpfx)tst-interpose-aux-thread.o
$(objpfx)tst-interpose-thread-malloc-check: $(objpfx)tst-interpose-aux-thread.o
$(objpfx)tst-interpose-static-nothread: $(objpfx)tst-interpose-aux-nothread.o
$(objpfx)tst-interpose-static-thread: \
  $(objpfx)tst-interpose-aux-thread.o $(static-thread-library)

tst-dynarray-ENV = MALLOC_TRACE=$(objpfx)tst-dynarray.mtrace \
		   LD_PRELOAD=$(objpfx)libc_malloc_debug.so
$(objpfx)tst-dynarray-mem.out: $(objpfx)tst-dynarray.out
	$(common-objpfx)malloc/mtrace $(objpfx)tst-dynarray.mtrace > $@; \
	$(evaluate-test)

tst-dynarray-fail-ENV = MALLOC_TRACE=$(objpfx)tst-dynarray-fail.mtrace \
			LD_PRELOAD=$(objpfx)libc_malloc_debug.so
$(objpfx)tst-dynarray-fail-mem.out: $(objpfx)tst-dynarray-fail.out
	$(common-objpfx)malloc/mtrace $(objpfx)tst-dynarray-fail.mtrace > $@; \
	$(evaluate-test)

tst-compathooks-on-ENV = LD_PRELOAD=$(objpfx)libc_malloc_debug.so
tst-compathooks-on-mcheck-ENV = LD_PRELOAD=$(objpfx)libc_malloc_debug.so
tst-compathooks-on-malloc-check-ENV = \
	LD_PRELOAD=$(objpfx)libc_malloc_debug.so
tst-mallocstate-ENV = LD_PRELOAD=$(objpfx)libc_malloc_debug.so
tst-mallocstate-malloc-check-ENV = LD_PRELOAD=$(objpfx)libc_malloc_debug.so

# The test needs malloc_get_state/malloc_set_state which is in
# libc_malloc_debug.so.
$(objpfx)tst-mallocstate: $(objpfx)libc_malloc_debug.so
$(objpfx)tst-mallocstate-malloc-check: $(objpfx)libc_malloc_debug.so
tst-mallocstate-threaded-main-ENV = LD_PRELOAD=$(objpfx)libc_malloc_debug.so
$(objpfx)tst-mallocstate-threaded-main: $(objpfx)libc_malloc_debug.so
tst-mallocstate-threaded-worker-ENV = LD_PRELOAD=$(objpfx)libc_malloc_debug.so
$(objpfx)tst-mallocstate-threaded-worker: $(objpfx)libc_malloc_debug.so

$(objpfx)tst-aligned-alloc-random.out: $(objpfx)tst-aligned_alloc-lib.so
$(objpfx)tst-aligned-alloc-random-thread.out: $(objpfx)tst-aligned_alloc-lib.so
$(objpfx)tst-aligned-alloc-random-thread-cross.out: $(objpfx)tst-aligned_alloc-lib.so
$(objpfx)tst-malloc-random.out: $(objpfx)tst-aligned_alloc-lib.so

tst-aligned-alloc-random-ENV = LD_PRELOAD=$(objpfx)tst-aligned_alloc-lib.so
tst-aligned-alloc-random-thread-ENV = LD_PRELOAD=$(objpfx)tst-aligned_alloc-lib.so
tst-aligned-alloc-random-thread-cross-ENV = LD_PRELOAD=$(objpfx)tst-aligned_alloc-lib.so
tst-malloc-random-ENV = LD_PRELOAD=$(objpfx)tst-aligned_alloc-lib.so

tst-free-aligned-sized-trace-ENV = MALLOC_TRACE=$(objpfx)tst-free-aligned-sized-mem.mtrace \
	LD_PRELOAD=$(objpfx)libc_malloc_debug.so
$(objpfx)tst-free-aligned-sized-mtrace.out: $(objpfx)tst-free-aligned-sized-trace.out
	$(common-objpfx)malloc/mtrace $(objpfx)tst-free-aligned-sized-mem.mtrace > $@; \
	$(evaluate-test)

tst-free-sized-trace-ENV = MALLOC_TRACE=$(objpfx)tst-free-sized-mem.mtrace \
	LD_PRELOAD=$(objpfx)libc_malloc_debug.so
$(objpfx)tst-free-sized-mtrace.out: $(objpfx)tst-free-sized-trace.out
	$(common-objpfx)malloc/mtrace $(objpfx)tst-free-sized-mem.mtrace > $@; \
	$(evaluate-test)
