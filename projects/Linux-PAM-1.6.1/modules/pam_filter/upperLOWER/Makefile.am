#
# Copyright (c) 2005 Thorsten Kukuk <kukuk@suse.de>
#

CLEANFILES = *~

securelibfilterdir = $(SECUREDIR)/pam_filter


AM_CFLAGS = -I$(top_srcdir)/libpam/include \
	-I$(srcdir)/.. @EXE_CFLAGS@ $(WARN_CFLAGS)
AM_LDFLAGS = @EXE_LDFLAGS@
LDADD = $(top_builddir)/libpam/libpam.la

securelibfilter_PROGRAMS = upperLOWER
