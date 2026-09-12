# Copyright (C) 1991-2026 Free Software Foundation, Inc.
# Copyright The GNU Toolchain Authors.
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

# List of markers for printf family function tests.
xprintf-funcs := p as d f s sn v vas vd vf vs vsn

# List of data types and formats for individual per-conversion printf tests.
fmt-convs := double ldouble
fmts := E e F f G g

# List of data types grouping all conversions in single printf tests.
nonfmt-convs := c char int llong long s short
nonfmt-convs += uchar uint ullong ulong ushort

convs := $(sort $(fmt-convs) $(nonfmt-convs))

xprintf-srcs := \
  $(foreach p,$(xprintf-funcs), \
	    $(foreach c,$(convs),tst-printf-format-$(p)-$(c)))

fmt-xprintf-stems := \
  $(foreach f,$(fmts), \
	    $(foreach p,$(xprintf-funcs), \
		      $(foreach c,$(fmt-convs), \
				tst-printf-format-$(p)-$(c)-$(f))))
nonfmt-xprintf-stems := \
  $(foreach p,$(xprintf-funcs), \
	    $(foreach c,$(nonfmt-convs),tst-printf-format-$(p)-$(c)))

xprintf-stems := $(sort $(fmt-xprintf-stems) $(nonfmt-xprintf-stems))

# List of markers for scanf family function tests.
xscanf-funcs := s f ss v vf vs

# List of data types and formats for individual per-conversion scanf tests.
# Further conversions are provided by sysdeps.
xscanf-kinds := char int real uint
fmt-xscanf-char-convs := c
fmts-xscanf-char := b c s
fmt-xscanf-int-convs := char llong short
fmts-xscanf-int := d i
fmt-xscanf-real-convs :=
fmts-xscanf-real := aa a ee e ff f gg g
fmt-xscanf-uint-convs := uchar ullong ushort
fmts-xscanf-uint := b o u xx x

xscanf-convs = $(sort $(fmt-xscanf-char-convs) $(fmt-xscanf-int-convs) \
		      $(fmt-xscanf-real-convs) $(fmt-xscanf-uint-convs))

xscanf-srcs = $(foreach p,$(xscanf-funcs), \
		$(foreach c,$(xscanf-convs),tst-scanf-format-$(p)-$(c)))

xscanf-stems = $(sort $(foreach k,$(xscanf-kinds), \
			$(foreach f,$(fmts-xscanf-$(k)), \
			  $(foreach p,$(xscanf-funcs), \
			    $(foreach c,$(fmt-xscanf-$(k)-convs), \
			      tst-scanf-format-$(p)-$(c)-$(f))))))

headers := \
  bits/printf-ldbl.h \
  bits/stdio_lim.h \
  printf.h \
  stdio_ext.h \
  # headers

routines := \
  _fitoa_word \
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
  tst-bz12701-c \
  tst-cookie \
  tst-dprintf-length \
  tst-fclose-devzero \
  tst-fclose-offset \
  tst-fdopen \
  tst-fdopen2 \
  tst-ferror \
  tst-fflush-all-input \
  tst-fflush-mmap \
  tst-fgets \
  tst-fgets2 \
  tst-fileno \
  tst-fmemopen \
  tst-fmemopen2 \
  tst-fmemopen3 \
  tst-fmemopen4 \
  tst-fopen \
  tst-fphex \
  tst-fphex-wide \
  tst-fread \
  tst-freopen2 \
  tst-freopen3 \
  tst-freopen4 \
  tst-freopen5 \
  tst-freopen6 \
  tst-freopen64-2 \
  tst-freopen64-3 \
  tst-freopen64-4 \
  tst-freopen64-6 \
  tst-freopen64-7 \
  tst-freopen7 \
  tst-fseek \
  tst-fseek-mmap \
  tst-fwrite \
  tst-fwrite-memstrm \
  tst-fwrite-overflow \
  tst-fwrite-pipe \
  tst-fwrite-pos \
  tst-fwrite-ro \
  tst-getline \
  tst-getline-enomem \
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
  tst-printf-macro \
  tst-printf-oct \
  tst-printf-round \
  tst-printfsz \
  tst-put-error \
  tst-read-offset \
  tst-renameat2 \
  tst-rndseek \
  tst-scanf-binary-c11 \
  tst-scanf-binary-c23 \
  tst-scanf-binary-gnu11 \
  tst-scanf-binary-gnu89 \
  tst-scanf-bz27650 \
  tst-scanf-bz34208 \
  tst-scanf-intn \
  tst-scanf-nan \
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
  tst-ungetc-fflush \
  tst-ungetc-leak \
  tst-ungetc-nomem \
  tst-unlockedio \
  tst-vfprintf-mbs-prec \
  tst-vfprintf-user-type \
  tst-vfprintf-width-i18n \
  tst-vfprintf-width-prec-alloc \
  tst-vfscanf-bz34008 \
  tst-wc-printf \
  tst-wscanf-bz34208 \
  tstdiomisc \
  tstgetln \
  tstscanf \
  xbug \
  # tests

# Clang 19 takes a very long time, it ran more than 27 minutes on Intel
# Core i7-1195G7 before the process was killed, to compile bug28.c.
# The issue was fixed in upstream but check if clang has the version that has the fix:
# https://github.com/llvm/llvm-project/commit/99dddef340e566e9d303010f1219f7d7d6d37a11
ifneq ($(have-test-clang-19-or-less),yes)
tests += \
  bug28 \
  # tests
endif

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
  tst-popen3 \
  tst-setvbuf2 \
  tst-setvbuf2-ind
  # tests-container

generated += \
  errlist-data-aux-shared.S \
  errlist-data-aux.S \
  siglist-aux-shared.S \
  siglist-aux.S \
  # generated

tests-internal = \
  tst-grouping_iterator \
  tst-setvbuf2 \
  tst-setvbuf2-ind \
  # tests-internal

test-srcs = \
  $(xprintf-srcs) \
  $(xscanf-srcs) \
  tst-fwrite-bz29459 \
  tst-printf \
  tst-printfsz-islongdouble \
  tst-unbputc \
  # test-srcs

ifeq ($(run-built-tests),yes)
tests-special += \
  $(foreach f,$(xprintf-stems),$(objpfx)$(f).out) \
  $(foreach f,$(xscanf-stems),$(objpfx)$(f).out) \
  $(objpfx)tst-fwrite-bz29459.out \
  $(objpfx)tst-printf.out \
  $(objpfx)tst-printfsz-islongdouble.out \
  $(objpfx)tst-setvbuf1-cmp.out \
  $(objpfx)tst-unbputc.out \
  # tests-special

ifeq (yes,$(build-shared))
ifneq ($(PERL),no)
tests-special += \
  $(foreach f,$(xprintf-stems),$(objpfx)$(f)-mem.out) \
  $(foreach f,$(xscanf-stems),$(objpfx)$(f)-mem.out) \
  $(objpfx)tst-freopen2-mem.out \
  $(objpfx)tst-freopen3-mem.out \
  $(objpfx)tst-freopen4-mem.out \
  $(objpfx)tst-freopen5-mem.out \
  $(objpfx)tst-freopen6-mem.out \
  $(objpfx)tst-freopen64-2-mem.out \
  $(objpfx)tst-freopen64-3-mem.out \
  $(objpfx)tst-freopen64-4-mem.out \
  $(objpfx)tst-freopen64-6-mem.out \
  $(objpfx)tst-getline-enomem-mem.out \
  $(objpfx)tst-getline-mem.out \
  $(objpfx)tst-printf-bz18872-mem.out \
  $(objpfx)tst-printf-bz25691-mem.out \
  $(objpfx)tst-printf-fp-free-mem.out \
  $(objpfx)tst-printf-fp-leak-mem.out \
  $(objpfx)tst-ungetc-leak-mem.out \
  $(objpfx)tst-vfprintf-width-prec-mem.out \
  # tests-special

generated += \
  $(foreach f,$(xprintf-stems),$(f)-mem.out) \
  $(foreach f,$(xprintf-stems),$(f).mtrace) \
  $(foreach f,$(xscanf-stems),$(f)-mem.out) \
  $(foreach f,$(xscanf-stems),$(f).mtrace) \
  tst-freopen2-mem.out \
  tst-freopen2.mtrace \
  tst-freopen3-mem.out \
  tst-freopen3.mtrace \
  tst-freopen4-mem.out \
  tst-freopen4.mtrace \
  tst-freopen5-mem.out \
  tst-freopen5.mtrace \
  tst-freopen6-mem.out \
  tst-freopen6.mtrace \
  tst-freopen64-2-mem.out \
  tst-freopen64-2.mtrace \
  tst-freopen64-3-mem.out \
  tst-freopen64-3.mtrace \
  tst-freopen64-4-mem.out \
  tst-freopen64-4.mtrace \
  tst-freopen64-6-mem.out \
  tst-freopen64-6.mtrace \
  tst-getline-enomem-mem.out \
  tst-getline-enomem.mtrace \
  tst-getline-mem.out \
  tst-getline.mtrace \
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
	$(filter-out -c,$(compile-command.c)) $(pic-cppflags) $(pic-ccflag) $(no-stack-protector) -S

$(objpfx)errlist-data-aux.S: errlist-data-gen.c
	$(make-target-directory)
	$(filter-out -c,$(compile-command.c)) $(pie-default) $(no-stack-protector) -S

ifndef no_deps
-include $(objpfx)errlist-data-aux.S.d $(objpfx)errlist-data-aux-shared.S.d
endif

$(objpfx)errlist-data.os: $(objpfx)errlist-data-aux-shared.S
$(addprefix $(objpfx)errlist-data, $(object-suffixes-noshared)): \
  $(objpfx)errlist-data-aux.S

$(objpfx)siglist-aux-shared.S: siglist-gen.c
	$(make-target-directory)
	$(filter-out -c,$(compile-command.c)) $(pic-cppflags) $(pic-ccflag) $(no-stack-protector) -S

$(objpfx)siglist-aux.S: siglist-gen.c
	$(make-target-directory)
	$(filter-out -c,$(compile-command.c)) $(pie-default) $(no-stack-protector) -S

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
tst-vfscanf-bz34008-ENV = \
  MALLOC_CHECK_=3 \
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
tst-getline-ENV = \
  MALLOC_TRACE=$(objpfx)tst-getline.mtrace \
  LD_PRELOAD=$(common-objpfx)malloc/libc_malloc_debug.so
tst-getline-enomem-ENV = \
  MALLOC_TRACE=$(objpfx)tst-getline-enomem.mtrace \
  LD_PRELOAD=$(common-objpfx)malloc/libc_malloc_debug.so
tst-freopen2-ENV = \
  MALLOC_TRACE=$(objpfx)tst-freopen2.mtrace \
  LD_PRELOAD=$(common-objpfx)malloc/libc_malloc_debug.so
tst-freopen64-2-ENV = \
  MALLOC_TRACE=$(objpfx)tst-freopen64-2.mtrace \
  LD_PRELOAD=$(common-objpfx)malloc/libc_malloc_debug.so
tst-freopen3-ENV = \
  MALLOC_TRACE=$(objpfx)tst-freopen3.mtrace \
  LD_PRELOAD=$(common-objpfx)malloc/libc_malloc_debug.so
tst-freopen64-3-ENV = \
  MALLOC_TRACE=$(objpfx)tst-freopen64-3.mtrace \
  LD_PRELOAD=$(common-objpfx)malloc/libc_malloc_debug.so
tst-freopen4-ENV = \
  MALLOC_TRACE=$(objpfx)tst-freopen4.mtrace \
  LD_PRELOAD=$(common-objpfx)malloc/libc_malloc_debug.so
tst-freopen64-4-ENV = \
  MALLOC_TRACE=$(objpfx)tst-freopen64-4.mtrace \
  LD_PRELOAD=$(common-objpfx)malloc/libc_malloc_debug.so
tst-freopen5-ENV = \
  MALLOC_TRACE=$(objpfx)tst-freopen5.mtrace \
  LD_PRELOAD=$(common-objpfx)malloc/libc_malloc_debug.so
tst-freopen6-ENV = \
  MALLOC_TRACE=$(objpfx)tst-freopen6.mtrace \
  LD_PRELOAD=$(common-objpfx)malloc/libc_malloc_debug.so
tst-freopen64-6-ENV = \
  MALLOC_TRACE=$(objpfx)tst-freopen64-6.mtrace \
  LD_PRELOAD=$(common-objpfx)malloc/libc_malloc_debug.so

$(objpfx)tst-fwrite-bz29459.out: tst-fwrite-bz29459.sh $(objpfx)tst-fwrite-bz29459
	$(SHELL) $< $(common-objpfx) '$(test-program-prefix)'; \
	$(evaluate-test)

$(objpfx)tst-unbputc.out: tst-unbputc.sh $(objpfx)tst-unbputc
	$(SHELL) $< $(common-objpfx) '$(test-program-prefix)'; \
	$(evaluate-test)

$(objpfx)tst-printf.out: tst-printf.sh $(objpfx)tst-printf
	$(SHELL) $< $(common-objpfx) '$(test-program-prefix)'; \
	$(evaluate-test)

# We can't split a quoted recipe line, so do it via an auxiliary variable.
make-tst-printf-format-out = \
  AWK='$(AWK)' BASH='$(BASH)' \
    $(BASH) $< $@ $(common-objpfx) \
    '$(run-program-prefix-before-env) \
     $(run-program-env) \
     MALLOC_TRACE=$(@:.out=.mtrace) \
     LD_PRELOAD=$(common-objpfx)malloc/libc_malloc_debug.so \
     $(run-program-prefix-after-env)'
$(objpfx)tst-printf-format-%.out: \
  tst-printf-format.sh $(foreach c,$(convs),tst-printf-format-$(c).sh) \
  $(foreach f,$(xprintf-srcs),$(objpfx)$(f)) tst-printf-format.awk
	$(make-tst-printf-format-out) > $@; \
	$(evaluate-test)

# With scanf format tests reuse the same executable for all the inputs
# of the same data type that use a different conversion specifier each.
define tst-scanf-format-rule
$$(objpfx)tst-scanf-format-$(1)-$(2)-$(3).out: \
	tst-scanf-format-$(2)-$(3).input $$(objpfx)tst-scanf-format-$(1)-$(2)
	$$(make-tst-scanf-format-out) > $$@ < $$<; \
	$$(evaluate-test)
endef
make-tst-scanf-format-out = \
  $(test-wrapper-env) \
  $(run-program-env) \
  MALLOC_TRACE=$(@:.out=.mtrace) \
  LD_PRELOAD=$(common-objpfx)malloc/libc_malloc_debug.so \
  $(host-test-program-cmd)
$(foreach k,$(xscanf-kinds), \
  $(foreach f,$(fmts-xscanf-$(k)), \
    $(foreach p,$(xscanf-funcs), \
      $(foreach c,$(fmt-xscanf-$(k)-convs), \
	$(eval $(call tst-scanf-format-rule,$(p),$(c),$(f)))))))

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
$(objpfx)tst-printf-format-%-mem.out: $(objpfx)tst-printf-format-%.out
	{ test -r $(objpfx)tst-printf-format-$*.mtrace \
	|| ( echo "$(objpfx)tst-printf-format-$*.mtrace does not exist"; \
	     exit 77; ) \
	&& $(common-objpfx)malloc/mtrace \
	  $(objpfx)tst-printf-format-$*.mtrace; } > $@; \
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

# Called during static library initialization, so turn stack-protection
# off for non-shared builds.
CFLAGS-_itoa.o = $(no-stack-protector)
CFLAGS-_itoa.op = $(no-stack-protector)
CFLAGS-_fitoa_word.o = $(no-stack-protector)
CFLAGS-_fitoa_word.op = $(no-stack-protector)

CFLAGS-scanf13.c += $(test-config-cflags-wno-fortify-source)

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
CFLAGS-tst-bz11319-fortify2.c += $(no-fortify-source) -D_FORTIFY_SOURCE=2

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

make-tst-freopen4-out = \
  $(run-program-prefix-before-env) \
  $(run-program-env) \
  MALLOC_TRACE=$(@:.out=.mtrace) \
  LD_PRELOAD=$(common-objpfx)malloc/libc_malloc_debug.so \
  $(run-program-prefix-after-env) $<

freopen4-temp-dir-before = warning: could not remove temporary file:
freopen4-temp-dir-after = : No such file or directory

$(objpfx)tst-freopen4.out: $(objpfx)tst-freopen4
	$(make-tst-freopen4-out) > $@ && \
	  grep "$(freopen4-temp-dir-before)" $@ \
	  | sed -e "s/$(freopen4-temp-dir-before)//; s/$(freopen4-temp-dir-after)//" \
	  | xargs rm -rf; \
	$(evaluate-test)

$(objpfx)tst-freopen64-4.out: $(objpfx)tst-freopen64-4
	$(make-tst-freopen4-out) > $@ && \
	  grep "$(freopen4-temp-dir-before)" $@ \
	  | sed -e "s/$(freopen4-temp-dir-before)//; s/$(freopen4-temp-dir-after)//" \
	  | xargs rm -rf; \
	$(evaluate-test)

$(objpfx)tst-setvbuf1.out: /dev/null $(objpfx)tst-setvbuf1
	$(test-program-cmd) > $@ 2>&1; \
	$(evaluate-test)

$(objpfx)tst-setvbuf1-cmp.out: tst-setvbuf1.expect $(objpfx)tst-setvbuf1.out
	cmp $^ > $@; \
	$(evaluate-test)

CFLAGS-tst-setvbuf2.c += -DIND_PROC=\"$(objpfx)tst-setvbuf2-ind\"
$(objpfx)tst-setvbuf2-ind : $(objpfx)tst-setvbuf2-ind.o $(shared-thread-library)
$(objpfx)tst-setvbuf2.out: $(objpfx)tst-setvbuf2-ind
$(objpfx)tst-setvbuf2 : $(shared-thread-library)

$(objpfx)tst-printf-round: $(libm)
$(objpfx)tst-scanf-round: $(libm)

# With scanf format tests real conversions depend on libm.
define tst-scanf-format-libm-dep
$$(objpfx)tst-scanf-format-$(1)-$(2): $$(libm)
endef
$(foreach p,$(xscanf-funcs), \
  $(foreach c,$(fmt-xscanf-real-convs), \
    $(eval $(call tst-scanf-format-libm-dep,$(p),$(c)))))

$(objpfx)tst-freopen7: $(shared-thread-library)
$(objpfx)tst-freopen64-7: $(shared-thread-library)
