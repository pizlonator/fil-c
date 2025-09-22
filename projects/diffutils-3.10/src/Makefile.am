# Automakefile for GNU diffutils programs.

# Copyright (C) 2001-2002, 2006, 2009-2013, 2015-2023 Free Software Foundation,
# Inc.

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

bin_PROGRAMS = cmp diff diff3 sdiff

localedir = $(datadir)/locale

AM_CPPFLAGS = -I../lib -I$(top_srcdir)/lib
AM_CFLAGS = $(WARN_CFLAGS) $(WERROR_CFLAGS)

LDADD = \
  libver.a \
  ../lib/libdiffutils.a \
  $(LIBCSTACK) \
  $(LIBINTL) \
  $(LIBICONV) \
  $(LIBSIGSEGV) \
  $(LIBTHREAD) \
  $(LIBUNISTRING) \
  $(CLOCK_TIME_LIB) \
  $(HARD_LOCALE_LIB) \
  $(MBRTOWC_LIB) \
  $(SETLOCALE_NULL_LIB)

diff_LDADD = $(LDADD)
cmp_LDADD = $(LDADD)
sdiff_LDADD = $(LDADD) $(GETRANDOM_LIB)
diff3_LDADD = $(LDADD)

cmp_SOURCES = cmp.c
diff3_SOURCES = diff3.c
sdiff_SOURCES = sdiff.c
diff_SOURCES = \
  analyze.c context.c diff.c dir.c ed.c ifdef.c io.c \
  normal.c side.c util.c
noinst_HEADERS =	\
  die.h			\
  diff.h		\
  system.h

MOSTLYCLEANFILES = paths.h paths.ht

cmp.$(OBJEXT) diff3.$(OBJEXT) diff.$(OBJEXT) sdiff.$(OBJEXT): paths.h

gdiff = `echo diff|sed '$(transform)'`
BUILT_SOURCES = paths.h
paths.h: Makefile.am
	$(AM_V_GEN)(echo '#define DEFAULT_DIFF_PROGRAM "'$(gdiff)'"' && \
	  echo '#define LOCALEDIR "$(localedir)"') >$@t && mv $@t $@

noinst_LIBRARIES = libver.a
nodist_libver_a_SOURCES = version.c version.h

BUILT_SOURCES += version.c
version.c: Makefile
	$(AM_V_GEN)rm -f $@
	$(AM_V_at)printf '#include <config.h>\n' > $@t
	$(AM_V_at)printf 'char const *Version = "$(PACKAGE_VERSION)";\n' >> $@t
	$(AM_V_at)chmod a-w $@t
	$(AM_V_at)mv $@t $@

BUILT_SOURCES += version.h
version.h: Makefile
	$(AM_V_GEN)rm -f $@
	$(AM_V_at)printf 'extern char const *Version;\n' > $@t
	$(AM_V_at)chmod a-w $@t
	$(AM_V_at)mv $@t $@

DISTCLEANFILES = version.c version.h
MAINTAINERCLEANFILES = $(BUILT_SOURCES)
