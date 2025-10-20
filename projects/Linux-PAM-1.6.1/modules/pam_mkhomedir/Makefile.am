#
# Copyright (c) 2005, 2006, 2009 Thorsten Kukuk <kukuk@suse.de>
# Copyright (c) 2008 Red Hat, Inc.
#

CLEANFILES = *~
MAINTAINERCLEANFILES = $(MANS) README

EXTRA_DIST = $(XMLS)

if HAVE_DOC
dist_man_MANS = pam_mkhomedir.8 mkhomedir_helper.8
endif
XMLS = README.xml pam_mkhomedir.8.xml mkhomedir_helper.8.xml
dist_check_SCRIPTS = tst-pam_mkhomedir
TESTS = $(dist_check_SCRIPTS) $(check_PROGRAMS)

securelibdir = $(SECUREDIR)
if HAVE_VENDORDIR
secureconfdir = $(VENDOR_SCONFIGDIR)
else
secureconfdir = $(SCONFIGDIR)
endif

AM_CFLAGS = -I$(top_srcdir)/libpam/include \
	-DMKHOMEDIR_HELPER=\"$(sbindir)/mkhomedir_helper\" $(WARN_CFLAGS)

securelib_LTLIBRARIES = pam_mkhomedir.la
pam_mkhomedir_la_SOURCES = pam_mkhomedir.c
pam_mkhomedir_la_LIBADD = $(top_builddir)/libpam_internal/libpam_internal.la \
			  $(top_builddir)/libpam/libpam.la
pam_mkhomedir_la_LDFLAGS = -no-undefined -avoid-version -module
if HAVE_VERSIONING
  pam_mkhomedir_la_LDFLAGS += -Wl,--version-script=$(srcdir)/../modules.map
endif

sbin_PROGRAMS = mkhomedir_helper
mkhomedir_helper_SOURCES = mkhomedir_helper.c
mkhomedir_helper_CFLAGS = $(AM_CFLAGS) @EXE_CFLAGS@
mkhomedir_helper_LDFLAGS = @EXE_LDFLAGS@
mkhomedir_helper_LDADD = $(top_builddir)/libpam/libpam.la

check_PROGRAMS = tst-pam_mkhomedir-retval
tst_pam_mkhomedir_retval_LDADD = $(top_builddir)/libpam/libpam.la

if ENABLE_REGENERATE_MAN
dist_noinst_DATA = README
-include $(top_builddir)/Make.xml.rules
endif
