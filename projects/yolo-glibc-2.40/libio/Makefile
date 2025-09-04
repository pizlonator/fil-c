# Copyright (C) 1995-2024 Free Software Foundation, Inc.
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
#	Specific makefile for libio.
#
subdir	:= libio

include ../Makeconfig

headers	:= stdio.h \
	   bits/stdio.h bits/stdio2.h bits/stdio2-decl.h bits/stdio-ldbl.h \
	   bits/types/FILE.h bits/types/__FILE.h bits/types/struct_FILE.h \
	   bits/types/__fpos_t.h bits/types/__fpos64_t.h \
	   bits/types/cookie_io_functions_t.h

routines	:=							      \
	filedoalloc iofclose iofdopen iofflush iofgetpos iofgets iofopen      \
	iofopncook iofputs iofread iofsetpos ioftell wfiledoalloc	      \
	iofwrite iogetdelim iogetline iogets iopadn iopopen ioputs	      \
	ioseekoff ioseekpos iosetbuffer iosetvbuf ioungetc		      \
	iovsprintf iovsscanf						      \
	iofgetpos64 iofopen64 iofsetpos64				      \
	fputwc fputwc_u getwc getwc_u getwchar getwchar_u iofgetws iofgetws_u \
	iofputws iofputws_u iogetwline iowpadn ioungetwc putwc putwc_u	      \
	putwchar putwchar_u putchar putchar_u fwprintf swprintf vwprintf      \
	wprintf wscanf fwscanf vwscanf vswprintf iovswscanf swscanf wgenops   \
	wstrops wfileops iofwide fwide wmemstream			      \
									      \
	clearerr feof ferror fileno fputc freopen fseek getc getchar	      \
	memstream pclose putc putchar rewind setbuf setlinebuf vasprintf      \
	iovdprintf vscanf vsnprintf obprintf fcloseall fseeko ftello	      \
	freopen64 fseeko64 ftello64					      \
									      \
	__fbufsize __freading __fwriting __freadable __fwritable __flbf	      \
	__fpurge __fpending __fsetlocking				      \
									      \
	libc_fatal fmemopen oldfmemopen vtables

gen-as-const-headers += libio-macros.sym

# Exclude fortified routines from being built with _FORTIFY_SOURCE
routines_no_fortify += \
  fwprintf \
  iofgets \
  iofgets_u \
  iofgetws \
  iofgetws_u \
  swprintf \
  vasprintf \
  vsnprintf \
  vswprintf \
  vwprintf \
  wprintf \
  # routines_no_fortify

tests = \
  bug-fopena+ \
  bug-fseek \
  bug-ftell \
  bug-memstream1 \
  bug-mmap-fflush \
  bug-rewind \
  bug-rewind2 \
  bug-ungetc \
  bug-ungetc2 \
  bug-ungetc3 \
  bug-ungetc4 \
  bug-ungetwc1 \
  bug-ungetwc2 \
  bug-wfflush \
  bug-wmemstream1 \
  bug-wsetpos \
  test-fmemopen \
  tst-atime \
  tst-bz22415 \
  tst-bz24051 \
  tst-bz24153 \
  tst-bz28828 \
  tst-closeall \
  tst-eof \
  tst-ext \
  tst-ext2 \
  tst-fdopen-seek-failure \
  tst-fgetc-after-eof \
  tst-fgetwc \
  tst-fgetws \
  tst-fopenloc2 \
  tst-fputws \
  tst-freopen \
  tst-fseek \
  tst-ftell-active-handler \
  tst-ftell-append \
  tst-ftell-partial-wide \
  tst-fwrite-error \
  tst-getdelim \
  tst-memstream1 \
  tst-memstream2 \
  tst-memstream3 \
  tst-memstream4 \
  tst-mmap-eofsync \
  tst-mmap-fflushsync \
  tst-mmap-offend \
  tst-mmap-setvbuf \
  tst-mmap2-eofsync \
  tst-popen1 \
  tst-setvbuf1 \
  tst-sprintf-chk-ub \
  tst-sprintf-ub \
  tst-sscanf \
  tst-swscanf \
  tst-ungetwc1 \
  tst-ungetwc2 \
  tst-wfile-sync \
  tst-widetext \
  tst-wmemstream1 \
  tst-wmemstream2 \
  tst-wmemstream3 \
  tst-wmemstream4 \
  tst-wmemstream5 \
  tst_getwc \
  tst_putwc \
  tst_swprintf \
  tst_swscanf \
  tst_wprintf \
  tst_wprintf2 \
  tst_wscanf \
  # tests

tests-internal = tst-vtables tst-vtables-interposed

ifeq (yes,$(build-shared))
# Add test-fopenloc only if shared library is enabled since it depends on
# shared localedata objects.
tests += tst-fopenloc
# Add tst-bz24228 only if shared library is enabled since it can never meet its
# objective with static linking because the relevant code just is not there.
tests += tst-bz24228
endif
test-srcs = test-freopen

ifeq ($(build-shared),yes)
routines += oldiofopen oldiofdopen oldiofclose oldiopopen oldpclose \
	    oldtmpfile oldiofgetpos oldiofgetpos64 oldiofsetpos	    \
	    oldiofsetpos64
endif

ifeq (yes,$(libc-reentrant))
routines += clearerr_u feof_u ferror_u fputc_u getc_u getchar_u		      \
	    iofflush_u putc_u putchar_u peekc iofread_u iofwrite_u iofgets_u  \
	    iofputs_u
endif

CPPFLAGS += $(libio-mtsafe)

# Support for exception handling.
CFLAGS-fileops.c += -fexceptions
CFLAGS-fputc.c += -fexceptions
CFLAGS-fputwc.c += -fexceptions
CFLAGS-freopen64.c += -fexceptions
CFLAGS-freopen.c += -fexceptions
CFLAGS-fseek.c += -fexceptions
CFLAGS-fseeko64.c += -fexceptions
CFLAGS-fseeko.c += -fexceptions
CFLAGS-ftello64.c += -fexceptions
CFLAGS-ftello.c += -fexceptions
CFLAGS-fwide.c += -fexceptions
CFLAGS-genops.c += -fexceptions
CFLAGS-getc.c += -fexceptions
CFLAGS-getchar.c += -fexceptions
CFLAGS-getwc.c += -fexceptions
CFLAGS-getwchar.c += -fexceptions
CFLAGS-iofclose.c += -fexceptions
CFLAGS-iofflush.c += -fexceptions $(config-cflags-wno-ignored-attributes)
CFLAGS-iofgetpos64.c += -fexceptions
CFLAGS-iofgetpos.c += -fexceptions
CFLAGS-iofgets.c += -fexceptions
CFLAGS-iofgetws.c += -fexceptions
CFLAGS-iofputs.c += -fexceptions $(config-cflags-wno-ignored-attributes)
CFLAGS-iofputws.c += -fexceptions
CFLAGS-iofread.c += -fexceptions
CFLAGS-iofsetpos64.c += -fexceptions
CFLAGS-iofsetpos.c += -fexceptions
CFLAGS-ioftell.c += -fexceptions
CFLAGS-iofwrite.c += -fexceptions $(config-cflags-wno-ignored-attributes)
CFLAGS-iogetdelim.c += -fexceptions
CFLAGS-iogetline.c += -fexceptions
CFLAGS-iogets.c += -fexceptions
CFLAGS-iogetwline.c += -fexceptions
CFLAGS-ioputs.c += -fexceptions
CFLAGS-ioseekoff.c += -fexceptions
CFLAGS-ioseekpos.c += -fexceptions
CFLAGS-iosetbuffer.c += -fexceptions
CFLAGS-iosetvbuf.c += -fexceptions
CFLAGS-ioungetc.c += -fexceptions
CFLAGS-ioungetwc.c += -fexceptions
CFLAGS-oldfileops.c += -fexceptions
CFLAGS-oldiofclose.c += -fexceptions
CFLAGS-oldiofgetpos64.c += -fexceptions
CFLAGS-oldiofgetpos.c += -fexceptions
CFLAGS-oldiofsetpos64.c += -fexceptions
CFLAGS-oldiofsetpos.c += -fexceptions
CFLAGS-peekc.c += -fexceptions
CFLAGS-putc.c += -fexceptions
CFLAGS-putchar.c += -fexceptions
CFLAGS-putwc.c += -fexceptions
CFLAGS-putwchar.c += -fexceptions
CFLAGS-rewind.c += -fexceptions
CFLAGS-wfileops.c += -fexceptions
CFLAGS-wgenops.c += -fexceptions
CFLAGS-oldiofopen.c += -fexceptions
CFLAGS-iofopen.c += -fexceptions
CFLAGS-iofopen64.c += -fexceptions
CFLAGS-oldtmpfile.c += -fexceptions
CFLAGS-fileno.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-feof_u.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-ferror_u.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-getc_u.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-iofflush_u.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-putc_u.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-iofgets_u.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-iofputs_u.c += $(config-cflags-wno-ignored-attributes)
# XXX Do we need filedoalloc and wfiledoalloc?  Others?

# Prevent fortification as these are built with -O0
CFLAGS-tst-bz24051.c += $(no-fortify-source)
CFLAGS-tst-bz24153.c += $(no-fortify-source)

CFLAGS-tst_putwc.c += -DOBJPFX=\"$(objpfx)\"

# These test cases intentionally use overlapping arguments
CFLAGS-tst-sprintf-ub.c += -Wno-restrict $(no-fortify-source)
CFLAGS-tst-sprintf-chk-ub.c += -Wno-restrict $(no-fortify-source)

LDFLAGS-tst-bz24228 = -Wl,--version-script=tst-bz24228.map

tst_wprintf2-ARGS = "Some Text"

test-fmemopen-ENV = MALLOC_TRACE=$(objpfx)test-fmemopen.mtrace \
		    LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so
tst-fdopen-seek-failure-ENV = \
  MALLOC_TRACE=$(objpfx)tst-fdopen-seek-failure.mtrace \
  LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so
tst-fopenloc-ENV = MALLOC_TRACE=$(objpfx)tst-fopenloc.mtrace \
		   LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so
tst-bz22415-ENV = MALLOC_TRACE=$(objpfx)tst-bz22415.mtrace \
		  LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so
tst-bz24228-ENV = MALLOC_TRACE=$(objpfx)tst-bz24228.mtrace \
		  LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so

aux	:= fileops genops stdfiles stdio strops

ifeq ($(run-built-tests),yes)
ifeq ($(build-shared),yes)
ifneq ($(PERL),no)
generated += \
  test-fmemopen.check \
  test-fmemopen.mtrace \
  tst-bz22415.check \
  tst-bz22415.mtrace \
  tst-bz24228.check \
  tst-bz24228.mtrace \
  tst-fdopen-seek-failure.check \
  tst-fdopen-seek-failure.mtrace \
  tst-fopenloc.check \
  tst-fopenloc.mtrace \
  # generated
endif
endif
endif

ifeq ($(build-shared),yes)
aux	+= oldfileops oldstdfiles
tests += \
  tst-fopen-compat \
  tst-stderr-compat \
# tests
tests-2.0 += \
  tst-fopen-compat \
  tst-stderr-compat \
# tests-2.0

tst-fopen-compat-ARGS = tst-fopen-compat.c
# Disable PIE to trigger copy relocation.
CFLAGS-tst-fopen-compat.c += -fno-pie
tst-fopen-compat-no-pie = yes
endif

shared-only-routines = oldiofopen oldiofdopen oldiofclose oldfileops	\
		       oldstdfiles oldiopopen oldpclose oldtmpfile	\
		       oldiofgetpos oldiofgetpos64 oldiofsetpos		\
		       oldiofsetpos64

ifeq ($(run-built-tests),yes)
tests-special += \
  $(objpfx)test-freopen.out \
  # tests-special
ifeq (yes,$(build-shared))
# Run tst-fopenloc-cmp.out and tst-openloc-mem.out only if shared
# library is enabled since they depend on tst-fopenloc.out.
tests-special += $(objpfx)tst-fopenloc-cmp.out
ifeq ($(build-shared),yes)
ifneq ($(PERL),no)
tests-special += \
  $(objpfx)test-fmemopen-mem.out \
  $(objpfx)tst-bz22415-mem.out \
  $(objpfx)tst-bz24228-mem.out \
  $(objpfx)tst-fdopen-seek-failure-mem.out \
  $(objpfx)tst-fopenloc-mem.out \
  # tests-special
endif
endif
endif

tests += \
  tst-cleanup-default \
  tst-cleanup-default-static \
  # tests
tests-static += tst-cleanup-default-static
tests-special += $(objpfx)tst-cleanup-default-cmp.out $(objpfx)tst-cleanup-default-static-cmp.out
LDFLAGS-tst-cleanup-default = -Wl,--gc-sections
LDFLAGS-tst-cleanup-default-static = -Wl,--gc-sections

ifeq ($(have-gnu-retain)$(have-z-start-stop-gc),yesyes)
tests += \
  tst-cleanup-nostart-stop-gc \
  tst-cleanup-nostart-stop-gc-static \
  tst-cleanup-start-stop-gc \
  tst-cleanup-start-stop-gc-static \
  # tests
tests-static += \
  tst-cleanup-nostart-stop-gc-static \
  tst-cleanup-start-stop-gc-static \
  # tests-static
tests-special += $(objpfx)tst-cleanup-start-stop-gc-cmp.out \
		$(objpfx)tst-cleanup-start-stop-gc-static-cmp.out \
		$(objpfx)tst-cleanup-nostart-stop-gc-cmp.out \
		$(objpfx)tst-cleanup-nostart-stop-gc-static-cmp.out
LDFLAGS-tst-cleanup-start-stop-gc := -Wl,--gc-sections,-z,start-stop-gc
LDFLAGS-tst-cleanup-start-stop-gc-static := -Wl,--gc-sections,-z,start-stop-gc
LDFLAGS-tst-cleanup-nostart-stop-gc := -Wl,--gc-sections,-z,nostart-stop-gc
LDFLAGS-tst-cleanup-nostart-stop-gc-static := -Wl,--gc-sections,-z,nostart-stop-gc
endif
endif

include ../Rules

ifeq ($(run-built-tests),yes)
LOCALES := de_DE.ISO-8859-1 de_DE.UTF-8 en_US.ISO-8859-1 en_US.UTF-8 \
	   ja_JP.EUC-JP ja_JP.UTF-8
include ../gen-locales.mk

$(objpfx)bug-ftell.out: $(gen-locales)
$(objpfx)bug-ungetwc1.out: $(gen-locales)
$(objpfx)bug-ungetwc2.out: $(gen-locales)
$(objpfx)tst-fgetwc.out: $(gen-locales)
$(objpfx)tst-fgetws.out: $(gen-locales)
$(objpfx)tst-fopenloc.out: $(gen-locales)
$(objpfx)tst-fputws.out: $(gen-locales)
$(objpfx)tst-fseek.out: $(gen-locales)
$(objpfx)tst-ftell-active-handler.out: $(gen-locales)
$(objpfx)tst-ftell-append.out: $(gen-locales)
$(objpfx)tst-ftell-partial-wide.out: $(gen-locales)
$(objpfx)tst-setvbuf1.out: $(gen-locales)
$(objpfx)tst-swscanf.out: $(gen-locales)
$(objpfx)tst-ungetwc1.out: $(gen-locales)
$(objpfx)tst-ungetwc2.out: $(gen-locales)
$(objpfx)tst-widetext.out: $(gen-locales)
$(objpfx)tst_wprintf2.out: $(gen-locales)
$(objpfx)tst-wfile-sync.out: $(gen-locales)
endif

define gen-tst-cleanup
$(objpfx)tst-cleanup-$1-cmp.out: tst-cleanup.exp $(objpfx)tst-cleanup-$1.out
	cmp $$^ > $$@; $$(evaluate-test)
endef

$(foreach t,default default-static start-stop-gc start-stop-gc-static nostart-stop-gc nostart-stop-gc-static, \
  $(eval $(call gen-tst-cleanup,$(t))))

$(objpfx)test-freopen.out: test-freopen.sh $(objpfx)test-freopen
	$(SHELL) $< $(common-objpfx) '$(test-program-prefix)'	\
	$(common-objpfx)libio/; \
	$(evaluate-test)

$(objpfx)tst-fopenloc-cmp.out: ../iconvdata/testdata/ISO-8859-1..UTF8 \
			       $(objpfx)tst-fopenloc.out
	cmp $^ > $@; \
	$(evaluate-test)

$(objpfx)test-fmemopen-mem.out: $(objpfx)test-fmemopen.out
	$(common-objpfx)malloc/mtrace $(objpfx)test-fmemopen.mtrace > $@; \
	$(evaluate-test)

$(objpfx)tst-fdopen-seek-failure-mem.out: $(objpfx)tst-fdopen-seek-failure.out
	$(common-objpfx)malloc/mtrace \
	  $(objpfx)tst-fdopen-seek-failure.mtrace > $@; \
	$(evaluate-test)

$(objpfx)tst-fopenloc-mem.out: $(objpfx)tst-fopenloc.out
	$(common-objpfx)malloc/mtrace $(objpfx)tst-fopenloc.mtrace > $@; \
	$(evaluate-test)

$(objpfx)tst-bz22415-mem.out: $(objpfx)tst-bz22415.out
	$(common-objpfx)malloc/mtrace $(objpfx)tst-bz22415.mtrace > $@; \
	$(evaluate-test)

$(objpfx)tst-bz24228-mem.out: $(objpfx)tst-bz24228.out
	$(common-objpfx)malloc/mtrace $(objpfx)tst-bz24228.mtrace > $@; \
	$(evaluate-test)
