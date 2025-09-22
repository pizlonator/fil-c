NULL =

AM_CPPFLAGS = \
	$(CODE_COVERAGE_CPPFLAGS) \
	-I$(top_srcdir)/src/libcommon \
	-I$(top_srcdir)/src/libkbdfile \
	-DDATADIR=\"$(realpath $(top_srcdir))/tests\" \
	-DBUILDDIR=\"$(realpath $(builddir))\"

AM_CFLAGS = $(CHECK_CFLAGS) $(CODE_COVERAGE_CFLAGS)

LDADD  = \
	$(top_builddir)/src/libcommon/libcommon.a \
	$(top_builddir)/src/libkbdfile/libkbdfile.la \
	@LIBINTL@ $(CODE_COVERAGE_LIBS)

noinst_PROGRAMS = \
	libkbdfile-test01 \
	libkbdfile-test02 \
	libkbdfile-test03 \
	libkbdfile-test04 \
	libkbdfile-test05 \
	libkbdfile-test06 \
	libkbdfile-test07 \
	libkbdfile-test08 \
	libkbdfile-test09 \
	libkbdfile-test10 \
	libkbdfile-test11 \
	libkbdfile-test12 \
	libkbdfile-test13 \
	$(NULL)
