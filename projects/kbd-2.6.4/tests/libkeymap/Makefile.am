NULL =

AM_CPPFLAGS = \
	$(CODE_COVERAGE_CPPFLAGS) \
	-I$(top_srcdir)/src/libcommon \
	-I$(top_srcdir)/src/libkbdfile \
	-I$(top_srcdir)/src/libkeymap \
	-DDATADIR=\"$(realpath $(top_srcdir))/tests\" \
	-DBUILDDIR=\"$(realpath $(builddir))\"

AM_CFLAGS = $(CHECK_CFLAGS) $(CODE_COVERAGE_CFLAGS)

LDADD  = \
	$(top_builddir)/src/libcommon/libcommon.a \
	$(top_builddir)/src/libkbdfile/libkbdfile.la \
	$(top_builddir)/src/libkeymap/libkeymap.la \
	@LIBINTL@ $(CODE_COVERAGE_LIBS)

noinst_PROGRAMS = \
	libkeymap-test01 \
	libkeymap-test02 \
	libkeymap-test03 \
	libkeymap-test04 \
	libkeymap-test05 \
	libkeymap-test06 \
	libkeymap-test07 \
	libkeymap-test08 \
	libkeymap-test09 \
	libkeymap-test10 \
	libkeymap-test11 \
	libkeymap-test12 \
	libkeymap-test13 \
	libkeymap-test14 \
	libkeymap-test15 \
	libkeymap-test16 \
	libkeymap-test17 \
	libkeymap-test18 \
	$(NULL)

