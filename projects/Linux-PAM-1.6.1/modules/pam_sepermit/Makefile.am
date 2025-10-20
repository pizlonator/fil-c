#
# Copyright (c) 2005, 2006, 2007 Thorsten Kukuk <kukuk@thkukuk.de>
# Copyright (c) 2008, 2009 Red Hat, Inc.
#

CLEANFILES = *~
MAINTAINERCLEANFILES = $(MANS) README

EXTRA_DIST = $(XMLS)

if HAVE_DOC
dist_man_MANS = pam_sepermit.8 sepermit.conf.5
endif
XMLS = README.xml pam_sepermit.8.xml sepermit.conf.5.xml
dist_check_SCRIPTS = tst-pam_sepermit
TESTS = $(dist_check_SCRIPTS) $(check_PROGRAMS)

securelibdir = $(SECUREDIR)
if HAVE_VENDORDIR
secureconfdir = $(VENDOR_SCONFIGDIR)
else
secureconfdir = $(SCONFIGDIR)
endif
sepermitlockdir = ${localstatedir}/run/sepermit

AM_CFLAGS = -I$(top_srcdir)/libpam/include \
	-D SEPERMIT_LOCKDIR=\"$(sepermitlockdir)\" $(WARN_CFLAGS)

pam_sepermit_la_LIBADD = $(top_builddir)/libpam/libpam.la @LIBSELINUX@
pam_sepermit_la_LDFLAGS = -no-undefined -avoid-version -module
if HAVE_VERSIONING
  pam_sepermit_la_LDFLAGS += -Wl,--version-script=$(srcdir)/../modules.map
endif

dist_secureconf_DATA = sepermit.conf
securelib_LTLIBRARIES = pam_sepermit.la

check_PROGRAMS = tst-pam_sepermit-retval
tst_pam_sepermit_retval_LDADD = $(top_builddir)/libpam/libpam.la

install-data-local:
	mkdir -p $(DESTDIR)$(sepermitlockdir)

if ENABLE_REGENERATE_MAN
dist_noinst_DATA = README
-include $(top_builddir)/Make.xml.rules
endif
