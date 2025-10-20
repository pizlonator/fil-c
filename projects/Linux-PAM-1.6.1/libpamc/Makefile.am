#
# Copyright (c) 2005 Thorsten Kukuk <kukuk@suse.de>
#

SUBDIRS = test

CLEANFILES = *~

EXTRA_DIST = License libpamc.map

include_HEADERS = include/security/pam_client.h

noinst_HEADERS = libpamc.h

AM_CFLAGS=-I$(top_srcdir)/libpam/include -I$(srcdir)/include $(WARN_CFLAGS)

libpamc_la_LDFLAGS = -no-undefined -version-info 82:1:82
if HAVE_VERSIONING
  libpamc_la_LDFLAGS += -Wl,--version-script=$(srcdir)/libpamc.map
endif
libpamc_la_LIBADD = $(top_builddir)/libpam_internal/libpam_internal.la

lib_LTLIBRARIES = libpamc.la

libpamc_la_SOURCES = pamc_client.c pamc_converse.c pamc_load.c

# Pkg-config script.
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = pamc.pc
