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
#	Makefile for stdlib routines
#
subdir	:= stdlib

include ../Makeconfig

headers := \
  alloca.h \
  bits/errno.h \
  bits/indirect-return.h \
  bits/monetary-ldbl.h \
  bits/stdint-intn.h \
  bits/stdint-least.h \
  bits/stdint-uintn.h \
  bits/stdlib-bsearch.h \
  bits/stdlib-float.h \
  bits/stdlib-ldbl.h \
  bits/stdlib.h \
  bits/time64.h \
  bits/timesize.h \
  bits/types/error_t.h \
  bits/wordsize.h \
  errno.h \
  fmtmsg.h \
  inttypes.h \
  monetary.h \
  stdbit.h \
  stdint.h \
  stdlib.h \
  sys/errno.h \
  sys/random.h \
  sys/ucontext.h \
  ucontext.h \
  # headers

routines := \
  a64l \
  abort \
  abs \
  arc4random \
  arc4random_uniform \
  at_quick_exit \
  atof \
  atoi \
  atol\
  atoll \
  bsearch \
  canonicalize \
  cxa_at_quick_exit \
  cxa_atexit \
  cxa_finalize \
  cxa_thread_atexit_impl \
  div \
  drand48 \
  drand48-iter \
  drand48_r \
  erand48 \
  erand48_r \
  exit \
  fmtmsg \
  getcontext \
  getentropy \
  getenv \
  getrandom \
  getsubopt \
  jrand48 \
  jrand48_r \
  l64a \
  labs \
  lcong48 \
  lcong48_r \
  ldiv \
  llabs \
  lldiv \
  lrand48 \
  lrand48_r \
  makecontext \
  mblen \
  mbstowcs \
  mbtowc \
  mrand48 \
  mrand48_r \
  nrand48 \
  nrand48_r \
  old_atexit  \
  on_exit atexit \
  putenv \
  qsort \
  quick_exit \
  rand \
  rand_r \
  random \
  random_r \
  rpmatch \
  secure-getenv \
  seed48 \
  seed48_r \
  setcontext \
  setenv \
  srand48 \
  srand48_r \
  stdc_bit_ceil_uc \
  stdc_bit_ceil_ui \
  stdc_bit_ceil_ul \
  stdc_bit_ceil_ull \
  stdc_bit_ceil_us \
  stdc_bit_floor_uc \
  stdc_bit_floor_ui \
  stdc_bit_floor_ul \
  stdc_bit_floor_ull \
  stdc_bit_floor_us \
  stdc_bit_width_uc \
  stdc_bit_width_ui \
  stdc_bit_width_ul \
  stdc_bit_width_ull \
  stdc_bit_width_us \
  stdc_count_ones_uc \
  stdc_count_ones_ui \
  stdc_count_ones_ul \
  stdc_count_ones_ull \
  stdc_count_ones_us \
  stdc_count_zeros_uc \
  stdc_count_zeros_ui \
  stdc_count_zeros_ul \
  stdc_count_zeros_ull \
  stdc_count_zeros_us \
  stdc_first_leading_one_uc \
  stdc_first_leading_one_ui \
  stdc_first_leading_one_ul \
  stdc_first_leading_one_ull \
  stdc_first_leading_one_us \
  stdc_first_leading_zero_uc \
  stdc_first_leading_zero_ui \
  stdc_first_leading_zero_ul \
  stdc_first_leading_zero_ull \
  stdc_first_leading_zero_us \
  stdc_first_trailing_one_uc \
  stdc_first_trailing_one_ui \
  stdc_first_trailing_one_ul \
  stdc_first_trailing_one_ull \
  stdc_first_trailing_one_us \
  stdc_first_trailing_zero_uc \
  stdc_first_trailing_zero_ui \
  stdc_first_trailing_zero_ul \
  stdc_first_trailing_zero_ull \
  stdc_first_trailing_zero_us \
  stdc_has_single_bit_uc \
  stdc_has_single_bit_ui \
  stdc_has_single_bit_ul \
  stdc_has_single_bit_ull \
  stdc_has_single_bit_us \
  stdc_leading_ones_uc \
  stdc_leading_ones_ui \
  stdc_leading_ones_ul \
  stdc_leading_ones_ull \
  stdc_leading_ones_us \
  stdc_leading_zeros_uc \
  stdc_leading_zeros_ui \
  stdc_leading_zeros_ul \
  stdc_leading_zeros_ull \
  stdc_leading_zeros_us \
  stdc_trailing_ones_uc \
  stdc_trailing_ones_ui \
  stdc_trailing_ones_ul \
  stdc_trailing_ones_ull \
  stdc_trailing_ones_us \
  stdc_trailing_zeros_uc \
  stdc_trailing_zeros_ui \
  stdc_trailing_zeros_ul \
  stdc_trailing_zeros_ull \
  stdc_trailing_zeros_us \
  strfmon \
  strfmon_l \
  strfromd \
  strfromf \
  strfroml \
  strtod \
  strtod_l \
  strtod_nan \
  strtof \
  strtof_l \
  strtof_nan \
  strtol \
  strtol_l \
  strtold \
  strtold_l \
  strtold_nan \
  strtoll \
  strtoll_l \
  strtoul \
  strtoul_l \
  strtoull \
  strtoull_l \
  swapcontext \
  system \
  wcstombs \
  wctomb  \
  xpg_basename \
  # routines

# Exclude fortified routines from being built with _FORTIFY_SOURCE
routines_no_fortify += \
  mbstowcs \
  wcstombs \
  wctomb \
  # routines_no_fortify

aux = \
  grouping \
  groupingwc \
  tens_in_limb \
  # aux

# These routines will be omitted from the libc shared object.
# Instead the static object files will be included in a special archive
# linked against when the shared library will be used.
static-only-routines = \
  at_quick_exit \
  atexit \
  # static-only-routines

test-srcs := \
  tst-fmtmsg \
  #test-srcs

tests := \
  bug-fmtmsg1 \
  bug-getcontext \
  bug-strtod \
  bug-strtod2 \
  test-a64l \
  test-at_quick_exit-race \
  test-atexit-race \
  test-atexit-recursive \
  test-bz22786 \
  test-canon \
  test-canon2 \
  test-cxa_atexit-race \
  test-cxa_atexit-race2 \
  test-dlclose-exit-race \
  test-on_exit-race \
  testdiv \
  testmb \
  testmb2 \
  testrand \
  testsort \
  tst-abs \
  tst-arc4random-fork \
  tst-arc4random-stats \
  tst-arc4random-thread \
  tst-at_quick_exit \
  tst-atexit \
  tst-atof1 \
  tst-atof2 \
  tst-bsearch \
  tst-bz20544 \
  tst-canon-bz26341 \
  tst-cxa_atexit \
  tst-environ \
  tst-getrandom \
  tst-labs \
  tst-limits \
  tst-llabs \
  tst-makecontext \
  tst-makecontext-align \
  tst-makecontext2 \
  tst-makecontext3 \
  tst-on_exit \
  tst-qsort \
  tst-qsort2 \
  tst-qsort3 \
  tst-qsort6 \
  tst-quick_exit \
  tst-rand48 \
  tst-rand48-2 \
  tst-random \
  tst-random2 \
  tst-realpath \
  tst-realpath-toolong \
  tst-secure-getenv \
  tst-setcontext \
  tst-setcontext2 \
  tst-setcontext3 \
  tst-setcontext4 \
  tst-setcontext5 \
  tst-setcontext6 \
  tst-setcontext7 \
  tst-setcontext8 \
  tst-setcontext9 \
  tst-setcontext10 \
  tst-setcontext11 \
  tst-stdbit-Wconversion \
  tst-stdbit-builtins \
  tst-stdc_bit_ceil \
  tst-stdc_bit_floor \
  tst-stdc_bit_width \
  tst-stdc_count_ones \
  tst-stdc_count_zeros \
  tst-stdc_first_leading_one \
  tst-stdc_first_leading_zero \
  tst-stdc_first_trailing_one \
  tst-stdc_first_trailing_zero \
  tst-stdc_has_single_bit \
  tst-stdc_leading_ones \
  tst-stdc_leading_zeros \
  tst-stdc_trailing_ones \
  tst-stdc_trailing_zeros \
  tst-strfmon_l \
  tst-strfrom \
  tst-strfrom-locale \
  tst-strtod \
  tst-strtod-nan-locale \
  tst-strtod-nan-sign \
  tst-strtod-overflow \
  tst-strtod-round \
  tst-strtod-underflow \
  tst-strtod2 \
  tst-strtod5 \
  tst-strtod6 \
  tst-strtol \
  tst-strtol-binary-c11 \
  tst-strtol-binary-c23 \
  tst-strtol-binary-gnu11 \
  tst-strtol-binary-gnu23 \
  tst-strtol-locale \
  tst-strtoll \
  tst-swapcontext1 \
  tst-swapcontext2 \
  tst-thread-quick_exit \
  tst-tininess \
  tst-unsetenv1 \
  tst-width \
  tst-width-stdint \
  tst-xpg-basename \
  # tests

tests-internal := \
  tst-qsort4 \
  tst-strtod1i \
  tst-strtod3 \
  tst-strtod4 \
  tst-strtod5i \
  tst-tls-atexit \
  tst-tls-atexit-nodelete \
  # tests-internal

tests-static := \
  tst-secure-getenv \
  # tests-static

tests-container := \
  tst-system \
  #tests-container

ifeq ($(build-hardcoded-path-in-tests),yes)
tests += \
  tst-empty-env \
  # tests
endif

LDLIBS-test-atexit-race = $(shared-thread-library)
LDLIBS-test-at_quick_exit-race = $(shared-thread-library)
LDLIBS-test-cxa_atexit-race = $(shared-thread-library)
LDLIBS-test-cxa_atexit-race2 = $(shared-thread-library)
LDLIBS-test-on_exit-race = $(shared-thread-library)
LDLIBS-tst-canon-bz26341 = $(shared-thread-library)
LDLIBS-tst-arc4random-fork = $(shared-thread-library)
LDLIBS-tst-arc4random-thread = $(shared-thread-library)
LDLIBS-tst-system = $(shared-thread-library)

LDLIBS-test-dlclose-exit-race = $(shared-thread-library)
LDFLAGS-test-dlclose-exit-race = $(LDFLAGS-rdynamic)
LDLIBS-test-dlclose-exit-race-helper.so = $(libsupport) $(shared-thread-library)

CFLAGS-tst-abs.c += -fno-builtin
CFLAGS-tst-labs.c += -fno-builtin
CFLAGS-tst-llabs.c += -fno-builtin

CFLAGS-tst-stdbit-Wconversion.c += -Wconversion -Werror
CFLAGS-tst-stdc_trailing_zeros.c += -fno-builtin
CFLAGS-tst-stdc_trailing_ones.c += -fno-builtin
CFLAGS-tst-stdc_leading_zeros.c += -fno-builtin
CFLAGS-tst-stdc_leading_ones.c += -fno-builtin
CFLAGS-tst-stdc_has_single_bit.c += -fno-builtin
CFLAGS-tst-stdc_first_trailing_zero.c += -fno-builtin
CFLAGS-tst-stdc_first_trailing_one.c += -fno-builtin
CFLAGS-tst-stdc_first_leading_zero.c += -fno-builtin
CFLAGS-tst-stdc_first_leading_one.c += -fno-builtin
CFLAGS-tst-stdc_count_zeros.c += -fno-builtin
CFLAGS-tst-stdc_count_ones.c += -fno-builtin
CFLAGS-tst-stdc_bit_width.c += -fno-builtin
CFLAGS-tst-stdc_bit_floor.c += -fno-builtin
CFLAGS-tst-stdc_bit_ceil.c += -fno-builtin

ifeq ($(have-cxx-thread_local),yes)
CFLAGS-tst-quick_exit.o = -std=c++11
LDLIBS-tst-quick_exit = -lstdc++
CFLAGS-tst-thread-quick_exit.o = -std=c++11
LDLIBS-tst-thread-quick_exit = -lstdc++
$(objpfx)tst-thread-quick_exit: $(shared-thread-library)
else
tests-unsupported += \
  tst-quick_exit \
  tst-thread-quick_exit \
  # tests-unsupported
endif

modules-names = \
  test-dlclose-exit-race-helper \
  tst-tls-atexit-lib \
  # modules-names
extra-test-objs += $(addsuffix .os, $(modules-names))

ifeq ($(build-shared),yes)
tests += \
  tst-putenv \
  # tests
endif

# Several mpn functions from GNU MP are used by the strtod function.
mpn-routines := \
  add_n \
  addmul_1 \
  cmp \
  divmod_1 \
  divrem \
  inlines \
  lshift \
  mod_1 \
  mul \
  mul_1 \
  mul_n \
  rshift \
  sub_n \
  submul_1 \
  udiv_qrnnd \
  # mpn-routines
mpn-headers = \
  asm-syntax.h \
  gmp-impl.h \
  gmp-mparam.h \
  gmp.h \
  longlong.h \
  # mpn-headers

routines := \
  $(strip $(routines) $(mpn-routines)) \
  dbl2mpn \
  ldbl2mpn \
  mpn2dbl \
  mpn2flt \
  mpn2ldbl \
  # routines
aux += \
  fpioconst \
  mp_clz_tab \
  # aux

tests-extras += \
  tst-putenvmod \
  # tests-extras

extra-test-objs += \
  tst-putenvmod.os \
  # extra-test-objs

generated += \
  isomac \
  isomac.out \
  tst-putenvmod.so \
  # generated

CFLAGS-bsearch.c += $(uses-callbacks)
CFLAGS-qsort.c += $(uses-callbacks)
CFLAGS-system.c += -fexceptions
CFLAGS-system.os = -fomit-frame-pointer
CFLAGS-fmtmsg.c += -fexceptions

CFLAGS-strfmon.c += $(libio-mtsafe)
CFLAGS-strfmon_l.c += $(libio-mtsafe)

# The strfrom class of functions call __printf_fp in order to convert the
# floating-point value to characters.  This requires the value of IO_MTSAFE_IO.
CFLAGS-strfromd.c += $(libio-mtsafe)
CFLAGS-strfromf.c += $(libio-mtsafe)
CFLAGS-strfroml.c += $(libio-mtsafe)

CFLAGS-strtol.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-strtoul.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-strtoll.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-strtoull.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-strtof.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-strtof_l.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-strtod.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-strtod_l.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-strtold.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-strtold_l.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-secure-getenv.c += $(config-cflags-wno-ignored-attributes)

CFLAGS-tst-bsearch.c += $(stack-align-test-flags)
CFLAGS-tst-qsort.c += $(stack-align-test-flags)
CFLAGS-tst-makecontext.c += -funwind-tables
CFLAGS-tst-makecontext2.c += $(stack-align-test-flags)

CFLAGS-testmb.c += -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -Wall -Werror

# Some versions of GCC supported for building glibc do not support -std=c23
# or -std=gnu23 (added in GCC 14), or the older names -std=c2x or -std=gnu2x
# (added in GCC 9), so the tests for those versions use -std=c11 and -std=gnu11
# and then _ISOC23_SOURCE is defined in the test as needed.
CFLAGS-tst-strtol-binary-c11.c += -std=c11
CFLAGS-tst-strtol-binary-c23.c += -std=c11
CFLAGS-tst-strtol-binary-gnu11.c += -std=gnu11
CFLAGS-tst-strtol-binary-gnu23.c += -std=gnu11


# Run a test on the header files we use.
tests-special += $(objpfx)isomac.out

ifeq ($(run-built-tests),yes)
tests-special += $(objpfx)tst-fmtmsg.out
endif

include ../Rules

ifeq ($(run-built-tests),yes)
LOCALES := \
  cs_CZ.UTF-8 \
  de_DE.UTF-8 \
  el_GR.UTF-8 \
  en_US.ISO-8859-1 \
  hi_IN.UTF-8 \
  hr_HR.UTF-8 \
  tg_TJ.UTF-8 \
  tr_TR.ISO-8859-9 \
  tr_TR.UTF-8 \
  # LOCALES
include ../gen-locales.mk

$(objpfx)bug-strtod2.out: $(gen-locales)
$(objpfx)testmb2.out: $(gen-locales)
$(objpfx)tst-strtod.out: $(gen-locales)
$(objpfx)tst-strtod1i.out: $(gen-locales)
$(objpfx)tst-strtod3.out: $(gen-locales)
$(objpfx)tst-strtod4.out: $(gen-locales)
$(objpfx)tst-strtod5.out: $(gen-locales)
$(objpfx)tst-strtod5i.out: $(gen-locales)
$(objpfx)tst-strtol-locale.out: $(gen-locales)
$(objpfx)tst-strtod-nan-locale.out: $(gen-locales)
$(objpfx)tst-strfmon_l.out: $(gen-locales)
$(objpfx)tst-strfrom.out: $(gen-locales)
$(objpfx)tst-strfrom-locale.out: $(gen-locales)
$(objpfx)test-dlclose-exit-race.out: $(objpfx)test-dlclose-exit-race-helper.so
endif

# Testdir has to be named stdlib and needs to be writable
test-canon-ARGS = --test-dir=${common-objpfx}stdlib

bug-fmtmsg1-ENV = SEV_LEVEL=foo,11,newsev

$(objpfx)isomac.out: $(objpfx)isomac
	$(dir $<)$(notdir $<) '$(CC)'  \
	'-I../include $(+sysdep-includes) $(sysincludes) -I..' > $@; \
	$(evaluate-test)

isomac-CFLAGS = -O
$(objpfx)isomac: isomac.c
	$(native-compile)

$(objpfx)tst-fmtmsg.out: tst-fmtmsg.sh $(objpfx)tst-fmtmsg
	$(SHELL) $< $(common-objpfx) '$(test-program-prefix-before-env)' \
		 '$(run-program-env)' '$(test-program-prefix-after-env)' \
		 $(common-objpfx)stdlib/; \
	$(evaluate-test)

$(objpfx)tst-putenv: $(objpfx)tst-putenvmod.so
LDFLAGS-tst-putenv = -Wl,--no-as-needed

$(objpfx)tst-putenvmod.so: $(objpfx)tst-putenvmod.os $(link-libc-deps)
	$(build-module)
libof-tst-putenvmod = extramodules

$(objpfx)bug-getcontext: $(libm)
$(objpfx)bug-strtod2: $(libm)
$(objpfx)tst-strtod-round: $(libm)
$(objpfx)tst-tininess: $(libm)
$(objpfx)tst-strtod-underflow: $(libm)
$(objpfx)tst-strtod5: $(libm)
$(objpfx)tst-strtod5i: $(libm)
$(objpfx)tst-strtod6: $(libm)
$(objpfx)tst-strtod-nan-locale: $(libm)
$(objpfx)tst-strtod-nan-sign: $(libm)

tst-tls-atexit-lib.so-no-z-defs = yes
test-dlclose-exit-race-helper.so-no-z-defs = yes

$(objpfx)tst-tls-atexit: $(shared-thread-library)
$(objpfx)tst-tls-atexit.out: $(objpfx)tst-tls-atexit-lib.so

$(objpfx)tst-tls-atexit-nodelete: $(shared-thread-library)
$(objpfx)tst-tls-atexit-nodelete.out: $(objpfx)tst-tls-atexit-lib.so

$(objpfx)tst-setcontext3.out: tst-setcontext3.sh $(objpfx)tst-setcontext3
	$(SHELL) $< $(common-objpfx) '$(test-program-prefix-before-env)' \
		 '$(run-program-env)' '$(test-program-prefix-after-env)' \
		 $(common-objpfx)stdlib/; \
	$(evaluate-test)

$(objpfx)tst-qsort5: $(libm)
