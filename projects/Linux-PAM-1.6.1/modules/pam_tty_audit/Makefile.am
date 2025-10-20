#
# Copyright (c) 2005, 2006, 2009, 2010 Thorsten Kukuk <kukuk@suse.de>
#

CLEANFILES = *~
MAINTAINERCLEANFILES = $(MANS) README

EXTRA_DIST = $(XMLS)

if HAVE_DOC
dist_man_MANS = pam_tty_audit.8
endif
XMLS = README.xml pam_tty_audit.8.xml
dist_check_SCRIPTS = tst-pam_tty_audit
TESTS = $(dist_check_SCRIPTS)

securelibdir = $(SECUREDIR)

AM_CFLAGS = -I$(top_srcdir)/libpam/include $(WARN_CFLAGS)
AM_LDFLAGS = -no-undefined -avoid-version -module
if HAVE_VERSIONING
  AM_LDFLAGS += -Wl,--version-script=$(srcdir)/../modules.map
endif

pam_tty_audit_la_LIBADD = $(top_builddir)/libpam/libpam.la
securelib_LTLIBRARIES = pam_tty_audit.la

if ENABLE_REGENERATE_MAN
dist_noinst_DATA = README
-include $(top_builddir)/Make.xml.rules
endif
