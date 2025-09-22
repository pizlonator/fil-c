AM_CPPFLAGS = \
	$(CODE_COVERAGE_CPPFLAGS) \
	-I$(srcdir) -I$(builddir) -I$(srcdir)/.. -I$(builddir)/.. \
	-I$(srcdir)/../libcommon -I$(builddir)/../libcommon \
	-DDATADIR=\"$(datadir)\" -DLOCALEDIR=\"$(localedir)\" \
	-U_GNU_SOURCE \
	-funit-at-a-time

AM_CFLAGS = $(CODE_COVERAGE_CFLAGS)
LDADD = \
	$(top_builddir)/src/libcommon/libcommon.a \
	$(CODE_COVERAGE_LIBS)

headers = \
	kbdfile.h

libkbdfile_la_SOURCES = \
	$(headers) \
	contextP.h \
	init.c \
	kbdfile.c

KBDFILE_CURRENT  = 1
KBDFILE_REVISION = 0
KBDFILE_AGE      = 0

if BUILD_LIBKBDFILE
lib_LTLIBRARIES = libkbdfile.la
nobase_include_HEADERS = $(headers)
libkbdfile_la_LDFLAGS = -export-symbols-regex ^kbdfile_.* -version-info $(KBDFILE_CURRENT):$(KBDFILE_REVISION):$(KBDFILE_AGE)
else
noinst_LTLIBRARIES = libkbdfile.la
endif
