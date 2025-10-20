#
# Copyright (c) 2008, 2009 Thorsten Kukuk <kukuk@suse.de>
# Copyright (c) 2013 Red Hat, Inc.
#

CLEANFILES = *~
MAINTAINERCLEANFILES = $(MANS) README

EXTRA_DIST = $(XMLS)

if HAVE_DOC
dist_man_MANS = pam_pwhistory.8 pwhistory.conf.5
if WITH_SELINUX
dist_man_MANS += pwhistory_helper.8
endif
endif

XMLS = README.xml pam_pwhistory.8.xml pwhistory.conf.5.xml
if WITH_SELINUX
XMLS += pwhistory_helper.8.xml
endif

dist_check_SCRIPTS = tst-pam_pwhistory
TESTS = $(dist_check_SCRIPTS) $(check_PROGRAMS)

securelibdir = $(SECUREDIR)
if HAVE_VENDORDIR
secureconfdir = $(VENDOR_SCONFIGDIR)
else
secureconfdir = $(SCONFIGDIR)
endif

AM_CFLAGS = -I$(top_srcdir)/libpam/include $(WARN_CFLAGS) \
	-DPWHISTORY_HELPER=\"$(sbindir)/pwhistory_helper\"

pam_pwhistory_la_LDFLAGS = -no-undefined -avoid-version -module
if HAVE_VERSIONING
  pam_pwhistory_la_LDFLAGS += -Wl,--version-script=$(srcdir)/../modules.map
endif

noinst_HEADERS = opasswd.h pwhistory_config.h

dist_secureconf_DATA = pwhistory.conf

securelib_LTLIBRARIES = pam_pwhistory.la
pam_pwhistory_la_CFLAGS = $(AM_CFLAGS)
pam_pwhistory_la_LIBADD = $(top_builddir)/libpam/libpam.la @LIBCRYPT@ @LIBSELINUX@
pam_pwhistory_la_SOURCES = pam_pwhistory.c opasswd.c pwhistory_config.c

if WITH_SELINUX
sbin_PROGRAMS = pwhistory_helper
pwhistory_helper_CFLAGS = $(AM_CFLAGS) -DHELPER_COMPILE=\"pwhistory_helper\" @EXE_CFLAGS@
pwhistory_helper_SOURCES = pwhistory_helper.c opasswd.c
pwhistory_helper_LDFLAGS = @EXE_LDFLAGS@
pwhistory_helper_LDADD = $(top_builddir)/libpam/libpam.la @LIBCRYPT@
endif

check_PROGRAMS = tst-pam_pwhistory-retval
tst_pam_pwhistory_retval_LDADD = $(top_builddir)/libpam/libpam.la

if ENABLE_REGENERATE_MAN
dist_noinst_DATA = README
-include $(top_builddir)/Make.xml.rules
endif
