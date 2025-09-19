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
#	Sub-makefile for wcsmbs portion of the library.
#
subdir	:= wcsmbs

include ../Makeconfig

headers := \
  bits/types/__mbstate_t.h \
  bits/types/mbstate_t.h \
  bits/types/wint_t.h \
  bits/wchar-ldbl.h \
  bits/wchar.h \
  bits/wchar2-decl.h \
  bits/wchar2.h \
  uchar.h \
  wchar.h \
  # headers

routines := \
  btowc \
  c16rtomb \
  c32rtomb \
  c8rtomb \
  isoc23_fwscanf \
  isoc23_swscanf \
  isoc23_vfwscanf \
  isoc23_vswscanf \
  isoc23_vwscanf \
  isoc23_wscanf \
  isoc99_fwscanf \
  isoc99_swscanf \
  isoc99_vfwscanf \
  isoc99_vswscanf \
  isoc99_vwscanf \
  isoc99_wscanf \
  mbrlen \
  mbrtoc8 \
  mbrtoc16 \
  mbrtoc32 \
  mbrtowc \
  mbsinit \
  mbsnrtowcs \
  mbsrtowcs \
  mbsrtowcs_l \
  wcpcpy \
  wcpncpy \
  wcrtomb \
  wcscasecmp \
  wcscasecmp_l \
  wcscat \
  wcschr \
  wcschrnul \
  wcscmp \
  wcscoll \
  wcscoll_l \
  wcscpy \
  wcscspn \
  wcsdup \
  wcslcat \
  wcslcpy \
  wcslen \
  wcsmbsload \
  wcsncase \
  wcsncase_l \
  wcsncat \
  wcsncmp \
  wcsncpy \
  wcsnlen \
  wcsnrtombs \
  wcspbrk \
  wcsrchr \
  wcsrtombs \
  wcsspn \
  wcsstr \
  wcstod \
  wcstod_l \
  wcstod_nan \
  wcstof \
  wcstof_l \
  wcstof_nan \
  wcstok \
  wcstol \
  wcstol_l \
  wcstold \
  wcstold_l \
  wcstold_nan \
  wcstoll \
  wcstoll_l \
  wcstoul \
  wcstoul_l \
  wcstoull \
  wcstoull_l \
  wcswidth \
  wcsxfrm \
  wcsxfrm_l \
  wctob \
  wcwidth \
  wmemchr \
  wmemcmp \
  wmemcpy \
  wmemmove \
  wmempcpy \
  wmemset \
  # routines

# Exclude fortified routines from being built with _FORTIFY_SOURCE
routines_no_fortify += \
  mbsnrtowcs \
  mbsrtowcs \
  wcpcpy \
  wcpncpy \
  wcrtomb \
  wcscat \
  wcscpy \
  wcslcat \
  wcslcpy \
  wcsncat \
  wcsncpy \
  wcsnrtombs \
  wcsrtombs \
  wmemcpy \
  wmemmove \
  wmempcpy \
  wmemset \
  # routines_no_fortify

tests := \
  test-c8rtomb \
  test-char-types \
  test-mbrtoc8 \
  test-wcpcpy \
  test-wcpncpy \
  test-wcscat \
  test-wcschr \
  test-wcschrnul \
  test-wcscmp \
  test-wcscpy \
  test-wcscspn \
  test-wcsdup \
  test-wcslen \
  test-wcsncat \
  test-wcsncmp \
  test-wcsncpy \
  test-wcsnlen \
  test-wcsnlen-nonarray \
  test-wcspbrk \
  test-wcsrchr \
  test-wcsspn \
  test-wcsstr \
  test-wmemchr \
  test-wmemcmp \
  test-wmemset \
  tst-btowc \
  tst-c16-surrogate \
  tst-c16c32-1 \
  tst-c32-state \
  tst-fgetwc-after-eof \
  tst-mbrtowc \
  tst-mbrtowc2 \
  tst-mbsrtowcs \
  tst-mbstowcs \
  tst-wchar-h \
  tst-wcpncpy \
  tst-wcrtomb \
  tst-wcslcat \
  tst-wcslcat2 \
  tst-wcslcpy \
  tst-wcslcpy2 \
  tst-wcsnlen \
  tst-wcstod-nan-locale \
  tst-wcstod-nan-sign \
  tst-wcstod-round \
  tst-wcstof \
  tst-wcstol-binary-c11 \
  tst-wcstol-binary-c23 \
  tst-wcstol-binary-gnu11 \
  tst-wcstol-binary-gnu23 \
  tst-wcstol-locale \
  tst-wprintf-binary \
  tst-wprintf-intn \
  tst-wscanf-binary-c11 \
  tst-wscanf-binary-c23 \
  tst-wscanf-binary-gnu11 \
  tst-wscanf-binary-gnu89 \
  tst-wscanf-intn \
  tst-wscanf-to_inpunct \
  wcsatcliff \
  wcsmbs-tst1 \
  # tests

# This test runs for a long time.
xtests += test-wcsncmp-nonarray


include ../Rules

ifeq ($(run-built-tests),yes)
LOCALES := \
  de_DE.ISO-8859-1 \
  de_DE.UTF-8 \
  en_US.ANSI_X3.4-1968 \
  fa_IR.UTF-8 \
  hr_HR.ISO-8859-2 \
  ja_JP.EUC-JP \
  tr_TR.ISO-8859-9 \
  tr_TR.UTF-8 \
  zh_HK.BIG5-HKSCS \
  zh_TW.EUC-TW \
  # LOCALES
include ../gen-locales.mk

$(objpfx)tst-btowc.out: $(gen-locales)
$(objpfx)tst-c16c32-1.out: $(gen-locales)
$(objpfx)tst-mbrtowc.out: $(gen-locales)
$(objpfx)tst-mbrtowc2.out: $(gen-locales)
$(objpfx)tst-wcrtomb.out: $(gen-locales)
$(objpfx)wcsmbs-tst1.out: $(gen-locales)
$(objpfx)tst-wcstol-locale.out: $(gen-locales)
$(objpfx)tst-wcstod-nan-locale.out: $(gen-locales)
$(objpfx)tst-c16-surrogate.out: $(gen-locales)
$(objpfx)tst-c32-state.out: $(gen-locales)
$(objpfx)test-c8rtomb.out: $(gen-locales)
$(objpfx)test-mbrtoc8.out: $(gen-locales)
$(objpfx)tst-wscanf-to_inpunct.out: $(gen-locales)
endif

$(objpfx)tst-wcstod-round: $(libm)

CFLAGS-wcwidth.c += -I../wctype
CFLAGS-wcswidth.c += -I../wctype

strtox-CFLAGS = -I../include
CFLAGS-wcstol.c += $(strtox-CFLAGS) $(config-cflags-wno-ignored-attributes)
CFLAGS-wcstoul.c += $(strtox-CFLAGS) $(config-cflags-wno-ignored-attributes)
CFLAGS-wcstoll.c += $(strtox-CFLAGS)
CFLAGS-wcstoull.c += $(strtox-CFLAGS)
CFLAGS-wcstod.c += $(strtox-CFLAGS) $(config-cflags-wno-ignored-attributes)
CFLAGS-wcstold.c += $(strtox-CFLAGS) $(config-cflags-wno-ignored-attributes)
CFLAGS-wcstof128.c += $(strtox-CFLAGS)
CFLAGS-wcstof.c += $(strtox-CFLAGS) $(config-cflags-wno-ignored-attributes)
CFLAGS-wcstol_l.c += $(strtox-CFLAGS)
CFLAGS-wcstoul_l.c += $(strtox-CFLAGS)
CFLAGS-wcstoll_l.c += $(strtox-CFLAGS)
CFLAGS-wcstoull_l.c += $(strtox-CFLAGS)
CFLAGS-wcstod_l.c += $(strtox-CFLAGS) $(config-cflags-wno-ignored-attributes)
CFLAGS-wcstold_l.c += $(strtox-CFLAGS) $(config-cflags-wno-ignored-attributes)
CFLAGS-wcstof128_l.c += $(strtox-CFLAGS)
CFLAGS-wcstof_l.c += $(strtox-CFLAGS) $(config-cflags-wno-ignored-attributes)
CPPFLAGS-tst-wchar-h.c += $(no-fortify-source) -D_FORTIFY_SOURCE=2

CFLAGS-wcschr.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-wmemchr.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-wmemset.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-mbrtowc.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-wcrtomb.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-wcstoll.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-wcstoull.c += $(config-cflags-wno-ignored-attributes)

CFLAGS-isoc99_wscanf.c += -fexceptions
CFLAGS-isoc99_fwscanf.c += -fexceptions
CFLAGS-isoc99_vwscanf.c += -fexceptions
CFLAGS-isoc99_vfwscanf.c += -fexceptions
CFLAGS-isoc23_wscanf.c += -fexceptions
CFLAGS-isoc23_fwscanf.c += -fexceptions
CFLAGS-isoc23_vwscanf.c += -fexceptions
CFLAGS-isoc23_vfwscanf.c += -fexceptions

CPPFLAGS += $(libio-mtsafe)

# We need to find the default version of strtold_l in stdlib.
CPPFLAGS-wcstold_l.c += -I../stdlib

$(objpfx)tst-wcstod-nan-locale: $(libm)
$(objpfx)tst-wcstod-nan-sign: $(libm)

# Some versions of GCC supported for building glibc do not support -std=c23
# or -std=gnu23 (added in GCC 14), or the older names -std=c2x or -std=gnu2x
# (added in GCC 9), so the tests for those versions use -std=c11 and -std=gnu11
# and then _ISOC23_SOURCE is defined in the test as needed.
CFLAGS-tst-wcstol-binary-c11.c += -std=c11
CFLAGS-tst-wcstol-binary-c23.c += -std=c11
CFLAGS-tst-wcstol-binary-gnu11.c += -std=gnu11
CFLAGS-tst-wcstol-binary-gnu23.c += -std=gnu11
CFLAGS-tst-wscanf-binary-c11.c += -std=c11 -DOBJPFX=\"$(objpfx)\"
CFLAGS-tst-wscanf-binary-c23.c += -std=c11 -DOBJPFX=\"$(objpfx)\"
CFLAGS-tst-wscanf-binary-gnu11.c += -std=gnu11 -DOBJPFX=\"$(objpfx)\"
CFLAGS-tst-wscanf-binary-gnu89.c += -std=gnu89 -DOBJPFX=\"$(objpfx)\"
