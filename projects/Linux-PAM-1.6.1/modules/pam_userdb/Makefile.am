#
# Copyright (c) 2005, 2006, 2009 Thorsten Kukuk <kukuk@thkukuk.de>
#

CLEANFILES = *~
MAINTAINERCLEANFILES = $(MANS) README

EXTRA_DIST = $(XMLS) create.pl

if HAVE_DOC
dist_man_MANS = pam_userdb.8
endif
XMLS = README.xml pam_userdb.8.xml
dist_check_SCRIPTS = tst-pam_userdb
TESTS = $(dist_check_SCRIPTS)

securelibdir = $(SECUREDIR)
if HAVE_VENDORDIR
secureconfdir = $(VENDOR_SCONFIGDIR)
else
secureconfdir = $(SCONFIGDIR)
endif

AM_CFLAGS = -I$(top_srcdir)/libpam/include $(WARN_CFLAGS)
AM_LDFLAGS = -no-undefined -avoid-version -module @LIBDB@ @LIBCRYPT@
if HAVE_VERSIONING
  AM_LDFLAGS += -Wl,--version-script=$(srcdir)/../modules.map
endif

securelib_LTLIBRARIES = pam_userdb.la
pam_userdb_la_LIBADD = $(top_builddir)/libpam/libpam.la

noinst_HEADERS = pam_userdb.h

if ENABLE_REGENERATE_MAN
dist_noinst_DATA = README
-include $(top_builddir)/Make.xml.rules
endif
