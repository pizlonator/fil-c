#
# Copyright (c) 2005, 2006, 2009 Thorsten Kukuk <kukuk@suse.de>
#

CLEANFILES = *~
MAINTAINERCLEANFILES = $(MANS) README

EXTRA_DIST = $(XMLS)

if HAVE_DOC
dist_man_MANS = limits.conf.5 pam_limits.8
endif
XMLS = README.xml limits.conf.5.xml pam_limits.8.xml
dist_check_SCRIPTS = tst-pam_limits
TESTS = $(dist_check_SCRIPTS)

securelibdir = $(SECUREDIR)
if HAVE_VENDORDIR
secureconfdir = $(VENDOR_SCONFIGDIR)
else
secureconfdir = $(SCONFIGDIR)
endif
limits_conf_dir = $(SCONFIGDIR)/limits.d

AM_CFLAGS = -I$(top_srcdir)/libpam/include \
	    -DLIMITS_FILE_DIR=\"$(limits_conf_dir)\" \
	    $(WARN_CFLAGS)
AM_LDFLAGS = -no-undefined -avoid-version -module
if HAVE_VERSIONING
  AM_LDFLAGS += -Wl,--version-script=$(srcdir)/../modules.map
endif

securelib_LTLIBRARIES = pam_limits.la
pam_limits_la_LIBADD = $(top_builddir)/libpam_internal/libpam_internal.la \
		       $(top_builddir)/libpam/libpam.la

dist_secureconf_DATA = limits.conf

install-data-local:
	mkdir -p $(DESTDIR)$(limits_conf_dir)

if ENABLE_REGENERATE_MAN
dist_noinst_DATA = README
-include $(top_builddir)/Make.xml.rules
endif
