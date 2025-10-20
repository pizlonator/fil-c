#
# Copyright (c) 2005, 2009 Thorsten Kukuk <kukuk@suse.de>
#

CLEANFILES = *~
MAINTAINERCLEANFILES = $(MANS) README

EXTRA_DIST = $(XMLS)

if HAVE_DOC
dist_man_MANS = pam_env.conf.5 pam_env.8 environment.5
endif
XMLS = README.xml pam_env.conf.5.xml pam_env.8.xml
dist_check_SCRIPTS = tst-pam_env
TESTS = $(dist_check_SCRIPTS) $(check_PROGRAMS)

securelibdir = $(SECUREDIR)
if HAVE_VENDORDIR
secureconfdir = $(VENDOR_SCONFIGDIR)
else
secureconfdir = $(SCONFIGDIR)
endif

AM_CFLAGS = -I$(top_srcdir)/libpam_internal/include \
	    -I$(top_srcdir)/libpam/include $(WARN_CFLAGS) \
	    -DSYSCONFDIR=\"$(sysconfdir)\" $(ECONF_CFLAGS)
AM_LDFLAGS = -no-undefined -avoid-version -module
if HAVE_VERSIONING
  AM_LDFLAGS += -Wl,--version-script=$(srcdir)/../modules.map
endif

securelib_LTLIBRARIES = pam_env.la
pam_env_la_LIBADD = $(top_builddir)/libpam_internal/libpam_internal.la \
		    $(top_builddir)/libpam/libpam.la $(ECONF_LIBS)

check_PROGRAMS = tst-pam_env-retval
tst_pam_env_retval_LDADD = $(top_builddir)/libpam/libpam.la

dist_secureconf_DATA = pam_env.conf
dist_sysconf_DATA = environment

if ENABLE_REGENERATE_MAN
dist_noinst_DATA = README
environment.5: pam_env.conf.5
-include $(top_builddir)/Make.xml.rules
endif
