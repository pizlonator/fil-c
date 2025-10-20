#
# Copyright (c) 2005, 2006, 2009 Thorsten Kukuk <kukuk@thkukuk.de>
#

CLEANFILES = *~
MAINTAINERCLEANFILES = $(MANS) README

EXTRA_DIST = $(XMLS)

if HAVE_DOC
dist_man_MANS = pam_echo.8
endif
XMLS = README.xml pam_echo.8.xml
dist_check_SCRIPTS = tst-pam_echo
TESTS = $(dist_check_SCRIPTS) $(check_PROGRAMS)

securelibdir = $(SECUREDIR)
if HAVE_VENDORDIR
secureconfdir = $(VENDOR_SCONFIGDIR)
else
secureconfdir = $(SCONFIGDIR)
endif

AM_CFLAGS = -I$(top_srcdir)/libpam/include $(WARN_CFLAGS)
AM_LDFLAGS = -no-undefined -avoid-version -module
if HAVE_VERSIONING
  AM_LDFLAGS += -Wl,--version-script=$(srcdir)/../modules.map
endif

securelib_LTLIBRARIES = pam_echo.la
pam_echo_la_LIBADD = $(top_builddir)/libpam/libpam.la

check_PROGRAMS = tst-pam_echo-retval
tst_pam_echo_retval_LDADD = $(top_builddir)/libpam/libpam.la

if ENABLE_REGENERATE_MAN
dist_noinst_DATA = README
-include $(top_builddir)/Make.xml.rules
endif
