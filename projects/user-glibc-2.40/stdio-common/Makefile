# Copyright (C) 1991-2024 Free Software Foundation, Inc.
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
#	Specific makefile for stdio-common.
#
subdir	:= stdio-common

include ../Makeconfig

headers := \
  bits/printf-ldbl.h \
  bits/stdio_lim.h \
  printf.h \
  stdio_ext.h \
  # headers

routines := \
  _itoa \
  _itowa \
  asprintf \
  ctermid \
  cuserid \
  dprintf \
  flockfile \
  fprintf \
  fscanf \
  ftrylockfile \
  funlockfile \
  gentempfd \
  getline \
  getw \
  grouping_iterator \
  iovfscanf \
  isoc23_fscanf \
  isoc23_scanf \
  isoc23_sscanf \
  isoc23_vfscanf \
  isoc23_vscanf \
  isoc23_vsscanf \
  isoc99_fscanf \
  isoc99_scanf \
  isoc99_sscanf \
  isoc99_vfscanf \
  isoc99_vscanf \
  isoc99_vsscanf \
  itoa-digits \
  itoa-udigits \
  itowa-digits \
  perror \
  printf \
  printf-prs \
  printf_buffer_as_file \
  printf_buffer_done \
  printf_buffer_flush \
  printf_buffer_pad_1 \
  printf_buffer_putc_1 \
  printf_buffer_puts_1 \
  printf_buffer_to_file \
  printf_buffer_write \
  printf_fp \
  printf_fphex \
  printf_function_invoke \
  printf_size \
  psiginfo \
  psignal \
  putw \
  reg-modifier \
  reg-printf \
  reg-type \
  remove \
  rename \
  renameat \
  renameat2 \
  scanf \
  snprintf \
  sprintf \
  sscanf \
  tempnam \
  tempname \
  tmpdir \
  tmpfile \
  tmpfile64 \
  tmpnam \
  tmpnam_r \
  translated_number_width \
  vfprintf \
  vfprintf-internal \
  vfscanf \
  vfscanf-internal \
  vfwprintf \
  vfwprintf-internal \
  vfwscanf \
  vfwscanf-internal \
  vprintf \
  wprintf_buffer_as_file \
  wprintf_buffer_done \
  wprintf_buffer_flush \
  wprintf_buffer_pad_1 \
  wprintf_buffer_putc_1 \
  wprintf_buffer_puts_1 \
  wprintf_buffer_to_file \
  wprintf_buffer_write \
  wprintf_function_invoke \
  # routines

# Exclude fortified routines from being built with _FORTIFY_SOURCE
routines_no_fortify += \
  asprintf \
  dprintf \
  fprintf \
  printf \
  snprintf \
  sprintf \
  vfprintf \
  vfwprintf \
  vprintf \
  # routines_no_fortify

aux := \
  errlist \
  errlist-data \
  errname \
  fxprintf \
  printf-parsemb \
  printf-parsewc \
  siglist \
  # aux

tests := \
  bug-vfprintf-nargs \
  bug1 \
  bug3 \
  bug4 \
  bug5 \
  bug6 \
  bug7 \
  bug8 \
  bug9 \
  bug10 \
  bug11 \
  bug12 \
  bug13 \
  bug14 \
  bug16 \
  bug17 \
  bug18 \
  bug18a \
  bug19 \
  bug19a \
  bug2 \
  bug20 \
  bug21 \
  bug22 \
  bug23 \
  bug24 \
  bug25 \
  bug26 \
  bug27 \
  bug28 \
  bug29 \
  errnobug \
  scanf1 \
  scanf2 \
  scanf3 \
  scanf4 \
  scanf5 \
  scanf7 \
  scanf8 \
  scanf9 \
  scanf10 \
  scanf11 \
  scanf12 \
  scanf13 \
  scanf14 \
  scanf15 \
  scanf16 \
  scanf17 \
  scanf18 \
  scanf19 \
  temptest \
  test-fseek \
  test-fwrite \
  test-popen \
  test-strerr \
  test-vfprintf \
  test_rdwr \
  tfformat \
  tiformat \
  tllformat \
  tst-bz11319 \
  tst-bz11319-fortify2 \
  tst-cookie \
  tst-dprintf-length \
  tst-fdopen \
  tst-fdopen2 \
  tst-ferror \
  tst-fgets \
  tst-fileno \
  tst-fmemopen \
  tst-fmemopen2 \
  tst-fmemopen3 \
  tst-fmemopen4 \
  tst-fphex \
  tst-fphex-wide \
  tst-fread \
  tst-fseek \
  tst-fwrite \
  tst-gets \
  tst-grouping \
  tst-grouping2 \
  tst-grouping3 \
  tst-long-dbl-fphex \
  tst-memstream-string \
  tst-obprintf \
  tst-perror \
  tst-popen \
  tst-popen2 \
  tst-printf-binary \
  tst-printf-intn \
  tst-printf-oct \
  tst-printf-round \
  tst-printfsz \
  tst-put-error \
  tst-renameat2 \
  tst-rndseek \
  tst-scanf-binary-c11 \
  tst-scanf-binary-c23 \
  tst-scanf-binary-gnu11 \
  tst-scanf-binary-gnu89 \
  tst-scanf-bz27650 \
  tst-scanf-intn \
  tst-scanf-round \
  tst-scanf-to_inpunct \
  tst-setvbuf1 \
  tst-sprintf \
  tst-sprintf-errno \
  tst-sprintf2 \
  tst-sprintf3 \
  tst-sscanf \
  tst-swprintf \
  tst-swscanf \
  tst-tmpnam \
  tst-ungetc \
  tst-ungetc-leak \
  tst-unlockedio \
  tst-vfprintf-mbs-prec \
  tst-vfprintf-user-type \
  tst-vfprintf-width-i18n \
  tst-vfprintf-width-prec-alloc \
  tst-wc-printf \
  tstdiomisc \
  tstgetln \
  tstscanf \
  xbug \
  # tests

ifeq ($(run-built-tests),yes)
ifeq (yes,$(build-shared))
ifneq ($(PERL),no)
tests += \
  tst-printf-bz18872 \
  tst-printf-bz25691 \
  tst-printf-fp-free \
  tst-printf-fp-leak \
  tst-vfprintf-width-prec \
  # tests
endif
endif
endif

tests-container += \
  tst-popen3
  # tests-container

generated += \
  errlist-data-aux-shared.S \
  errlist-data-aux.S \
  siglist-aux-shared.S \
  siglist-aux.S \
  # generated

tests-internal = \
  tst-grouping_iterator \
  # tests-internal

test-srcs = \
  tst-printf \
  tst-printfsz-islongdouble \
  tst-unbputc \
  # test-srcs

ifeq ($(run-built-tests),yes)
tests-special += \
  $(objpfx)tst-printf.out \
  $(objpfx)tst-printfsz-islongdouble.out \
  $(objpfx)tst-setvbuf1-cmp.out \
  $(objpfx)tst-unbputc.out \
  # tests-special

ifeq (yes,$(build-shared))
ifneq ($(PERL),no)
tests-special += \
  $(objpfx)tst-printf-bz18872-mem.out \
  $(objpfx)tst-printf-bz25691-mem.out \
  $(objpfx)tst-printf-fp-free-mem.out \
  $(objpfx)tst-printf-fp-leak-mem.out \
  $(objpfx)tst-ungetc-leak-mem.out \
  $(objpfx)tst-vfprintf-width-prec-mem.out \
  # tests-special

generated += \
  tst-printf-bz18872-mem.out \
  tst-printf-bz18872.c \
  tst-printf-bz18872.mtrace \
  tst-printf-bz25691-mem.out \
  tst-printf-bz25691.mtrace \
  tst-printf-fp-free-mem.out \
  tst-printf-fp-free.mtrace \
  tst-printf-fp-leak-mem.out \
  tst-printf-fp-leak.mtrace \
  tst-scanf-bz27650.mtrace \
  tst-ungetc-leak-mem.out \
  tst-ungetc-leak.mtrace \
  tst-vfprintf-width-prec-mem.out \
  tst-vfprintf-width-prec.mtrace \
  # generated
endif
endif
endif # $(run-built-tests)

tests-special += $(objpfx)tst-errno-manual.out

include ../Rules

# The errlist.c is built in two phases because compiler might reorder the
# compat_symbol directive prior the object itself and on binutils older
# than 2.29 it might generate object sizes different than the expected ones.
$(objpfx)errlist-data-aux-shared.S: errlist-data-gen.c
	$(make-target-directory)
	$(compile-command.c) $(pic-cppflags) $(pic-ccflag) $(no-stack-protector) -S

$(objpfx)errlist-data-aux.S: errlist-data-gen.c
	$(make-target-directory)
	$(compile-command.c) $(pie-default) $(no-stack-protector) -S

ifndef no_deps
-include $(objpfx)errlist-data-aux.S.d $(objpfx)errlist-data-aux-shared.S.d
endif

$(objpfx)errlist-data.os: $(objpfx)errlist-data-aux-shared.S
$(addprefix $(objpfx)errlist-data, $(object-suffixes-noshared)): \
  $(objpfx)errlist-data-aux.S

$(objpfx)siglist-aux-shared.S: siglist-gen.c
	$(make-target-directory)
	$(compile-command.c) $(pic-cppflags) $(pic-ccflag) $(no-stack-protector) -S

$(objpfx)siglist-aux.S: siglist-gen.c
	$(make-target-directory)
	$(compile-command.c) $(pie-default) $(no-stack-protector) -S

ifndef no_deps
-include $(objpfx)siglist-aux.S.d $(objpfx)siglist-aux-shared.S.d
endif

$(objpfx)siglist.os: $(objpfx)siglist-aux-shared.S
$(addprefix $(objpfx)siglist, $(object-suffixes-noshared)): \
  $(objpfx)siglist-aux.S

ifeq ($(run-built-tests),yes)
LOCALES := \
  bn_BD.UTF-8 \
  de_DE.ISO-8859-1 \
  de_DE.UTF-8 \
  en_US.ISO-8859-1 \
  fa_IR.UTF-8 \
  hi_IN.UTF-8 \
  ja_JP.EUC-JP \
  ps_AF.UTF-8 \
  rw_RW.UTF-8 \
  tg_TJ.UTF-8 \
  unm_US.UTF-8 \
  # LOCALES
include ../gen-locales.mk

$(objpfx)bug14.out: $(gen-locales)
$(objpfx)scanf13.out: $(gen-locales)
$(objpfx)test-vfprintf.out: $(gen-locales)
$(objpfx)tst-grouping.out: $(gen-locales)
$(objpfx)tst-grouping2.out: $(gen-locales)
$(objpfx)tst-grouping_iterator.out: $(gen-locales)
$(objpfx)tst-sprintf.out: $(gen-locales)
$(objpfx)tst-sscanf.out: $(gen-locales)
$(objpfx)tst-swprintf.out: $(gen-locales)
$(objpfx)tst-vfprintf-mbs-prec.out: $(gen-locales)
$(objpfx)tst-vfprintf-width-i18n.out: $(gen-locales)
$(objpfx)tst-grouping3.out: $(gen-locales)
$(objpfx)tst-scanf-to_inpunct.out: $(gen-locales)
endif

tst-printf-bz18872-ENV = MALLOC_TRACE=$(objpfx)tst-printf-bz18872.mtrace \
			LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so
tst-vfprintf-width-prec-ENV = \
  MALLOC_TRACE=$(objpfx)tst-vfprintf-width-prec.mtrace \
  LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so
tst-printf-bz25691-ENV = \
  MALLOC_TRACE=$(objpfx)tst-printf-bz25691.mtrace \
  LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so
tst-printf-fp-free-ENV = \
  MALLOC_TRACE=$(objpfx)tst-printf-fp-free.mtrace \
  LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so
tst-printf-fp-leak-ENV = \
  MALLOC_TRACE=$(objpfx)tst-printf-fp-leak.mtrace \
  LD_PRELOAD=$(common-objpfx)/malloc/libc_malloc_debug.so
tst-scanf-bz27650-ENV = \
  MALLOC_TRACE=$(objpfx)tst-scanf-bz27650.mtrace \
  LD_PRELOAD=$(common-objpfx)malloc/libc_malloc_debug.so
tst-ungetc-leak-ENV = \
  MALLOC_TRACE=$(objpfx)tst-ungetc-leak.mtrace \
  LD_PRELOAD=$(common-objpfx)malloc/libc_malloc_debug.so

$(objpfx)tst-unbputc.out: tst-unbputc.sh $(objpfx)tst-unbputc
	$(SHELL) $< $(common-objpfx) '$(test-program-prefix)'; \
	$(evaluate-test)

$(objpfx)tst-printf.out: tst-printf.sh $(objpfx)tst-printf
	$(SHELL) $< $(common-objpfx) '$(test-program-prefix)'; \
	$(evaluate-test)

$(objpfx)tst-printfsz-islongdouble.out: \
  tst-printfsz-islongdouble.sh $(objpfx)tst-printfsz-islongdouble
	$(SHELL) $^ '$(test-program-prefix)' $@; \
	$(evaluate-test)

# We generate this source because it requires a printf invocation with
# 10K arguments.
$(objpfx)tst-printf-bz18872.c: tst-printf-bz18872.sh
	rm -f $@ && $(BASH) $^ > $@.new && mv $@.new $@

$(objpfx)tst-%-mem.out: $(objpfx)tst-%.out
	$(common-objpfx)malloc/mtrace $(objpfx)tst-$*.mtrace > $@; \
	$(evaluate-test)

errlist-h = $(firstword $(wildcard $(addsuffix /errlist.h,$(sysdirs) .)))

$(objpfx)tst-errno-manual.out: tst-errno-manual.py \
			       $(errlist-h) \
			       $(..)manual/errno.texi
	$(PYTHON) tst-errno-manual.py -m $(..)manual/errno.texi \
				      -e $(errlist-h) > $@; \
	$(evaluate-test)

CFLAGS-vfprintf.c += -Wno-uninitialized
CFLAGS-vfwprintf.c += -Wno-uninitialized

CFLAGS-tmpfile.c += -fexceptions
CFLAGS-tmpfile64.c += -fexceptions
CFLAGS-tempname.c += -fexceptions
CFLAGS-psignal.c += -fexceptions
CFLAGS-vprintf.c += -fexceptions
CFLAGS-cuserid.c += -fexceptions

CFLAGS-vfprintf.c += -fexceptions
CFLAGS-fprintf.c += -fexceptions
CFLAGS-printf.c += -fexceptions
CFLAGS-vfwprintf.c += -fexceptions
CFLAGS-vfscanf.c += -fexceptions
CFLAGS-vfwscanf.c += -fexceptions
CFLAGS-fscanf.c += -fexceptions
CFLAGS-scanf.c += -fexceptions
CFLAGS-isoc99_vfscanf.c += -fexceptions
CFLAGS-isoc99_vscanf.c += -fexceptions
CFLAGS-isoc99_fscanf.c += -fexceptions
CFLAGS-isoc99_scanf.c += -fexceptions
CFLAGS-isoc23_vfscanf.c += -fexceptions
CFLAGS-isoc23_vscanf.c += -fexceptions
CFLAGS-isoc23_fscanf.c += -fexceptions
CFLAGS-isoc23_scanf.c += -fexceptions

CFLAGS-dprintf.c += $(config-cflags-wno-ignored-attributes)

# Called during static library initialization, so turn stack-protection
# off for non-shared builds.
CFLAGS-_itoa.o = $(no-stack-protector)
CFLAGS-_itoa.op = $(no-stack-protector)

# scanf18.c and scanf19.c test a deprecated extension which is no
# longer visible under most conformance levels; see the source files
# for more detail.
CFLAGS-scanf18.c += -std=gnu89
CFLAGS-scanf19.c += -std=gnu89

CFLAGS-bug3.c += -DOBJPFX=\"$(objpfx)\"
CFLAGS-bug4.c += -DOBJPFX=\"$(objpfx)\"
CFLAGS-bug5.c += -DOBJPFX=\"$(objpfx)\"
CFLAGS-test-fseek.c += -DOBJPFX=\"$(objpfx)\"
CFLAGS-test-popen.c += -DOBJPFX=\"$(objpfx)\"
CFLAGS-test_rdwr.c += -DOBJPFX=\"$(objpfx)\"

# tst-gets.c tests a deprecated function.
CFLAGS-tst-gets.c += -Wno-deprecated-declarations

# BZ #11319 was first fixed for regular vdprintf, then reopened because
# the fortified version had the same bug.
CFLAGS-tst-bz11319-fortify2.c += $(no-fortify-source),-D_FORTIFY_SOURCE=2

CFLAGS-tst-memstream-string.c += -fno-builtin-fprintf

# Some versions of GCC supported for building glibc do not support -std=c23
# (added in GCC 14), or the older name -std=c2x (added in GCC 9), so
# the test for that version uses -std=c11 and then _ISOC23_SOURCE is defined in
# the test as needed.
CFLAGS-tst-scanf-binary-c11.c += -std=c11 -DOBJPFX=\"$(objpfx)\"
CFLAGS-tst-scanf-binary-c23.c += -std=c11 -DOBJPFX=\"$(objpfx)\"
CFLAGS-tst-scanf-binary-gnu11.c += -std=gnu11 -DOBJPFX=\"$(objpfx)\"
CFLAGS-tst-scanf-binary-gnu89.c += -std=gnu89 -DOBJPFX=\"$(objpfx)\"

CPPFLAGS += $(libio-mtsafe)

$(objpfx)tst-setvbuf1.out: /dev/null $(objpfx)tst-setvbuf1
	$(test-program-cmd) > $@ 2>&1; \
	$(evaluate-test)

$(objpfx)tst-setvbuf1-cmp.out: tst-setvbuf1.expect $(objpfx)tst-setvbuf1.out
	cmp $^ > $@; \
	$(evaluate-test)

$(objpfx)tst-printf-round: $(libm)
$(objpfx)tst-scanf-round: $(libm)
