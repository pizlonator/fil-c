#
# Copyright (c) 2006 Thorsten Kukuk <kukuk@suse.de>
#

AM_CFLAGS = -DLIBPAM_COMPILE -I$(top_srcdir)/libpam/include \
	-I$(top_srcdir)/libpamc/include -I$(top_srcdir)/libpam_misc/include \
	$(WARN_CFLAGS)
LDADD = $(top_builddir)/libpam/libpam.la \
	$(top_builddir)/libpam_misc/libpam_misc.la

CLEANFILES = *~ $(XTESTS)

EXTRA_DIST = run-xtests.sh tst-pam_dispatch1.pamd tst-pam_dispatch2.pamd \
	tst-pam_dispatch3.pamd tst-pam_dispatch4.pamd \
	tst-pam_dispatch5.pamd \
	tst-pam_unix1.pamd tst-pam_unix2.pamd tst-pam_unix3.pamd \
	tst-pam_unix4.pamd \
	tst-pam_unix1.sh tst-pam_unix2.sh tst-pam_unix3.sh \
	tst-pam_unix4.sh \
	access.conf tst-pam_access1.pamd tst-pam_access1.sh \
	tst-pam_access2.pamd tst-pam_access2.sh \
	tst-pam_access3.pamd tst-pam_access3.sh \
	tst-pam_access4.pamd tst-pam_access4.sh \
	limits.conf tst-pam_limits1.pamd tst-pam_limits1.sh \
	tst-pam_succeed_if1.pamd tst-pam_succeed_if1.sh \
	group.conf tst-pam_group1.pamd tst-pam_group1.sh \
	tst-pam_authfail.pamd tst-pam_authsucceed.pamd \
	tst-pam_shells.pamd shells.conf tst-pam_shells.sh \
	tst-pam_substack1.pamd tst-pam_substack1a.pamd tst-pam_substack1.sh \
	tst-pam_substack2.pamd tst-pam_substack2a.pamd tst-pam_substack2.sh \
	tst-pam_substack3.pamd tst-pam_substack3a.pamd tst-pam_substack3.sh \
	tst-pam_substack4.pamd tst-pam_substack4a.pamd tst-pam_substack4.sh \
	tst-pam_substack5.pamd tst-pam_substack5a.pamd tst-pam_substack5.sh \
	tst-pam_assemble_line1.pamd tst-pam_assemble_line1.sh \
	tst-pam_pwhistory1.pamd tst-pam_pwhistory1.sh \
	tst-pam_time1.pamd time.conf \
	tst-pam_motd.sh tst-pam_motd1.sh tst-pam_motd2.sh \
	tst-pam_motd3.sh tst-pam_motd4.sh tst-pam_motd1.pamd \
	tst-pam_motd2.pamd tst-pam_motd3.pamd tst-pam_motd4.pamd

XTESTS = tst-pam_dispatch1 tst-pam_dispatch2 tst-pam_dispatch3 \
	tst-pam_dispatch4 tst-pam_dispatch5 \
	tst-pam_unix1 tst-pam_unix2 tst-pam_unix3 tst-pam_unix4 \
	tst-pam_access1 tst-pam_access2 tst-pam_access3 \
	tst-pam_access4 tst-pam_limits1 tst-pam_succeed_if1 \
	tst-pam_group1 tst-pam_authfail tst-pam_authsucceed \
	tst-pam_pwhistory1 tst-pam_time1 tst-pam_motd \
	tst-pam_shells

NOSRCTESTS = tst-pam_substack1 tst-pam_substack2 tst-pam_substack3 \
	tst-pam_substack4 tst-pam_substack5 tst-pam_assemble_line1


EXTRA_PROGRAMS = $(XTESTS)

xtests: $(XTESTS) run-xtests.sh
	"$(srcdir)"/run-xtests.sh "$(srcdir)" ${XTESTS} ${NOSRCTESTS}
