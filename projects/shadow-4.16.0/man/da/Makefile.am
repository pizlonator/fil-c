
mandir = @mandir@/da

# 2012.01.28 - activate manpages with more than 50% translated messages
man_MANS = \
	man1/chfn.1 \
	man8/groupdel.8 \
	man1/groups.1 \
	man5/gshadow.5 \
	man8/logoutd.8 \
	man1/newgrp.1 \
	man8/nologin.8 \
	man1/sg.1 \
	man8/vigr.8 \
	man8/vipw.8

man_nopam = 

if !USE_PAM
man_MANS += $(man_nopam)
endif

EXTRA_DIST = \
	$(man_MANS) \
	man1/id.1

if USE_PAM
EXTRA_DIST += $(man_nopam)
endif

include ../generate_translations.mak

