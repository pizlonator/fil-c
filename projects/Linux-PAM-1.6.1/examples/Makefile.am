#
# Copyright (c) 2005 Thorsten Kukuk <kukuk@suse.de>
#

CLEANFILES = *~

EXTRA_DIST = README

AM_CFLAGS = -I$(top_srcdir)/libpam/include -I$(top_srcdir)/libpamc/include \
	-I$(top_srcdir)/libpam_misc/include $(WARN_CFLAGS)
LDADD = $(top_builddir)/libpam/libpam.la \
	$(top_builddir)/libpam_misc/libpam_misc.la

noinst_PROGRAMS = xsh vpass blank check_user tty_conv
