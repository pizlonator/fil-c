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
#	Makefile for locales.
#
subdir	:= locale

include ../Makeconfig

headers = \
  bits/locale.h \
  bits/types/__locale_t.h \
  bits/types/locale_t.h \
  langinfo.h \
  locale.h \
  # headers
routines = \
  duplocale \
  findlocale \
  freelocale \
  loadarchive \
  loadlocale \
  localeconv \
  mb_cur_max \
  newlocale \
  nl_langinfo \
  nl_langinfo_l \
  setlocale \
  uselocale
  # routines
tests = \
  tst-C-locale \
  tst-duplocale \
  tst-locname \
  # tests
tests-container	= \
  tst-localedef-path-norm \
  # tests-container
categories = \
  address \
  collate \
  ctype \
  identification \
  measurement \
  messages \
  monetary \
  name \
  numeric \
  paper \
  telephone \
  time \
  # categories
aux = \
  $(categories:%=C-%) \
  $(categories:%=lc-%) \
  C_name \
  SYS_libc \
  coll-lookup \
  global-locale \
  localename \
  xlocale \
  # aux
others = \
  locale \
  localedef \
  # others
#others-static = \
# localedef \
# locale \
# # others-static
install-bin = \
  locale \
  localedef \
  # install-bin
extra-objs = \
  $(lib-modules:=.o) \
  $(locale-modules:=.o) \
  $(localedef-aux:=.o) \
  $(localedef-modules:=.o) \
  # extra-objs
generated += C-translit.h
before-compile += $(objpfx)C-translit.h

extra-libs = libBrokenLocale
extra-libs-others = $(extra-libs)

libBrokenLocale-routines = broken_cur_max

subdir-dirs = programs
vpath %.c programs
vpath %.h programs
vpath %.gperf programs

localedef-modules := \
  charmap \
  linereader \
  localedef $(categories:%=ld-%) \
  locarchive \
  locfile \
  repertoire \
  # localedef-modules
localedef-aux := md5
locale-modules := \
  locale \
  locale-spec \
  # locale-modules
lib-modules := \
  charmap-dir \
  record-status \
  simple-hash \
  xasprintf \
  xmalloc \
  xstrdup \
  # lib-modules

GPERF = gperf
GPERFFLAGS = -acCgopt -k1,2,5,9,$$ -L ANSI-C

ifeq ($(run-built-tests),yes)
tests-special += $(objpfx)tst-locale-locpath.out
endif

include ../Rules

programs/%-kw.h: programs/%-kw.gperf
	cd programs \
	&& $(GPERF) $(GPERFFLAGS) -N $(@F:-kw.h=_hash) $(<F) > $(@F).new
	mv -f $@.new $@

$(objpfx)localedef: $(localedef-modules:%=$(objpfx)%.o)
$(objpfx)localedef: $(localedef-aux:%=$(objpfx)%.o)
$(objpfx)locale: $(locale-modules:%=$(objpfx)%.o)
$(objpfx)localedef $(objpfx)locale: $(lib-modules:%=$(objpfx)%.o)

$(objpfx)C-translit.h: C-translit.h.in gen-translit.py
	$(make-target-directory)
	$(PYTHON) gen-translit.py < $< > $@.tmp
	mv -f $@.tmp $@

# The path to the compiled binary locale archive or compiled locales,
# along with the parent path to the source locales and source
# charmaps.
localepath = "$(complocaledir):$(i18ndir)"

# -Iprograms doesn't really belong here, but this gets it at the head
# of the list instead of the tail, where CPPFLAGS-$(lib) gets added.
# We need it before the standard -I's to see programs/config.h first.
locale-CPPFLAGS = -DCOMPLOCALEDIR='"$(complocaledir)"' \
		  -DLOCALE_ALIAS_PATH='"$(localedir)"' \
		  -Iprograms

CPPFLAGS-locale-programs = -DLOCALE_PATH='$(localepath)' \
			   -DCHARMAP_PATH='"$(i18ndir)/charmaps"' \
			   -DREPERTOIREMAP_PATH='"$(i18ndir)/repertoiremaps"' \
			   -DLOCSRCDIR='"$(i18ndir)/locales"'

CFLAGS-charmap.c += -Wno-write-strings -Wno-char-subscripts
CFLAGS-locfile.c += -Wno-write-strings -Wno-char-subscripts
CFLAGS-charmap-dir.c += -Wno-write-strings

# Set libof-* for each routine.
cpp-srcs-left := $(localedef-modules) $(localedef-aux) $(locale-modules) \
		 $(lib-modules)
lib := locale-programs
include $(patsubst %,$(..)libof-iterator.mk,$(cpp-srcs-left))

$(objpfx)tst-locale-locpath.out : tst-locale-locpath.sh $(objpfx)locale
	$(SHELL) $< '$(common-objpfx)' '$(test-wrapper-env)' '$(run-program-env)' > $@; \
	$(evaluate-test)

$(objpfx)tst-localedef-path-norm: $(shared-thread-library)
