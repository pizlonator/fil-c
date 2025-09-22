# SPDX-License-Identifier: LGPL-2.0-or-later
#
# Copyright (C) 2020  Alexey Gladkov <gladkov.alexey@gmail.com>
#
AM_CPPFLAGS = \
	$(CODE_COVERAGE_CPPFLAGS) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/libcommon \
	-I$(top_srcdir)/src/libkbdfile \
	-DDATADIR=\"$(datadir)\" \
	-funit-at-a-time

AM_CFLAGS = $(CODE_COVERAGE_CFLAGS)

CLEANFILES = libkfont.pc
EXTRA_DIST = libkfont.pc.in libkfont.map

LDADD = \
	$(top_builddir)/src/libcommon/libcommon.a \
	$(CODE_COVERAGE_LIBS)

KFONT_CURRENT  = 2
KFONT_REVISION = 0
KFONT_AGE      = 0

KFONT_SHAREDLIB = libkfont.so
KFONT_SONAME    = $(KFONT_SHAREDLIB).$(KFONT_CURRENT)
KFONT_MAPFILE   = $(top_srcdir)/src/libkfont/libkfont.map

libkfont_la_SOURCES = \
	kfont.h kfontP.h \
	psffontop.c \
	psfxtable.c \
	context.c \
	unicode.c \
	utf8.c utf8.h \
	kdmapop.c \
	loadunimap.c \
	mapscrn.c \
	setfont.c \
	kdfontop.c

libkfont_la_LIBADD = $(builddir)/../libkbdfile/libkbdfile.la
libkfont_la_LDFLAGS = -Wl,-soname,$(KFONT_SONAME),--version-script=$(KFONT_MAPFILE),-z,defs,-stats

if BUILD_LIBKFONT
pkgconfigdir   = $(libdir)/pkgconfig
pkgconfig_DATA = libkfont.pc

nobase_include_HEADERS = kfont.h
lib_LTLIBRARIES = libkfont.la
else
noinst_LTLIBRARIES = libkfont.la
endif

%.pc: %.pc.in Makefile
	$(SED) \
	    -e 's,[@]VERSION[@],$(VERSION),g' \
	    -e 's,[@]prefix[@],$(prefix),g' \
	    -e 's,[@]exec_prefix[@],$(exec_prefix),g' \
	    -e 's,[@]libdir[@],$(libdir),g' \
	    -e 's,[@]includedir[@],$(includedir),g' \
	    <$< >$(builddir)/$@
	touch -r $< $(builddir)/$@
	chmod 644 $(builddir)/$@
