#
# Copyright (c) 2005, 2006, 2007 Thorsten Kukuk <kukuk@thkukuk.de>
#

CLEANFILES = *~
MAINTAINERCLEANFILES = $(MANS) README

EXTRA_DIST = $(XMLS) pam_selinux_check.8

if HAVE_DOC
dist_man_MANS = pam_selinux.8
endif
XMLS = README.xml pam_selinux.8.xml
dist_check_SCRIPTS = tst-pam_selinux
TESTS = $(dist_check_SCRIPTS)

securelibdir = $(SECUREDIR)
if HAVE_VENDORDIR
secureconfdir = $(VENDOR_SCONFIGDIR)
else
secureconfdir = $(SCONFIGDIR)
endif

AM_CFLAGS = -I$(top_srcdir)/libpam/include $(WARN_CFLAGS)

pam_selinux_la_LDFLAGS = -no-undefined -avoid-version -module
pam_selinux_la_LIBADD = $(top_builddir)/libpam/libpam.la @LIBSELINUX@ @LIBAUDIT@
if HAVE_VERSIONING
  pam_selinux_la_LDFLAGS += -Wl,--version-script=$(srcdir)/../modules.map
endif

securelib_LTLIBRARIES = pam_selinux.la
noinst_PROGRAMS = pam_selinux_check
pam_selinux_check_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/libpamc/include \
			   -I$(top_srcdir)/libpam_misc/include
pam_selinux_check_LDADD = $(top_builddir)/libpam/libpam.la \
			  $(top_builddir)/libpam_misc/libpam_misc.la

if ENABLE_REGENERATE_MAN
dist_noinst_DATA = README
-include $(top_builddir)/Make.xml.rules
endif
