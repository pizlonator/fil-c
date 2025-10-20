#
# Copyright (c) 2005 Thorsten Kukuk <kukuk@suse.de>
#

CLEANFILES = *~

EXTRA_DIST = README

AM_YFLAGS = -d

AM_CFLAGS = -I$(top_srcdir)/libpam/include $(WARN_CFLAGS)

pam_conv1_CFLAGS = $(AM_CFLAGS) -Wno-unused-function -Wno-sign-compare

BUILT_SOURCES = pam_conv_y.h

noinst_PROGRAMS = pam_conv1

pam_conv1_SOURCES = pam_conv_l.l pam_conv_y.y
