#
# Copyright (c) 2006, 2009 Thorsten Kukuk <kukuk@suse.de>
#

AM_CFLAGS = -DLIBPAM_COMPILE -I$(top_srcdir)/libpam/include \
		-I$(top_srcdir)/libpam $(WARN_CFLAGS)
LDADD = $(top_builddir)/libpam_internal/libpam_internal.la \
	$(top_builddir)/libpam/libpam.la

CLEANFILES = *~

TESTS = tst-pam_start tst-pam_end tst-pam_fail_delay tst-pam_open_session \
	tst-pam_close_session tst-pam_acct_mgmt tst-pam_authenticate \
	tst-pam_chauthtok tst-pam_setcred tst-pam_get_item tst-pam_set_item \
	tst-pam_getenvlist tst-pam_get_user tst-pam_get_data tst-pam_set_data \
	tst-pam_mkargv tst-pam_start_confdir

EXTRA_DIST = confdir

check_PROGRAMS = ${TESTS} tst-dlopen

tst_dlopen_LDADD = $(top_builddir)/libpam_internal/libpam_internal.la -ldl
