#
# Copyright (c) 2005, 2006, 2009 Thorsten Kukuk <kukuk@suse.de>
#

CLEANFILES = *~
MAINTAINERCLEANFILES = $(MANS) README

EXTRA_DIST = $(XMLS)

if HAVE_DOC
dist_man_MANS = time.conf.5 pam_time.8
endif
XMLS = README.xml time.conf.5.xml pam_time.8.xml
dist_check_SCRIPTS = tst-pam_time
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
pam_time_la_LIBADD = $(top_builddir)/libpam_internal/libpam_internal.la \
		     $(top_builddir)/libpam/libpam.la

securelib_LTLIBRARIES = pam_time.la
dist_secureconf_DATA = time.conf

check_PROGRAMS = tst-pam_time-retval
tst_pam_time_retval_LDADD = $(top_builddir)/libpam/libpam.la

if ENABLE_REGENERATE_MAN
dist_noinst_DATA = README
-include $(top_builddir)/Make.xml.rules
endif
