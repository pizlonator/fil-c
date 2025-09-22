NULL =

AM_CPPFLAGS = \
	$(CODE_COVERAGE_CPPFLAGS) \
	-I$(top_srcdir)/src/libcommon \
	-I$(top_srcdir)/src/libkeymap \
	-I$(top_srcdir)/src/libkbdfile \
	-DDATADIR=\"$(realpath $(top_srcdir))/tests\" \
	-DBUILDDIR=\"$(realpath $(builddir))\"

AM_CFLAGS = $(CHECK_CFLAGS) $(CODE_COVERAGE_CFLAGS)

LDADD  = \
	$(top_builddir)/src/libcommon/libcommon.a \
	$(top_builddir)/src/libkeymap/libkeymap.la \
	$(top_builddir)/src/libkbdfile/libkbdfile.la \
	@LIBINTL@ $(CODE_COVERAGE_LIBS)

noinst_PROGRAMS = \
	libkeymap-bkeymap  \
	libkeymap-dumpkeys \
	libkeymap-mktable  \
	libkeymap-showmaps \
	$(NULL)
