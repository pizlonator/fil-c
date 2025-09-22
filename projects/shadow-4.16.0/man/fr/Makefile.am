
mandir = @mandir@/fr

man_MANS = \
	man1/chage.1 \
	man1/chfn.1 \
	man8/chgpasswd.8 \
	man8/chpasswd.8 \
	man1/chsh.1 \
	man1/expiry.1 \
	man5/faillog.5 \
	man8/faillog.8 \
	man3/getspnam.3 \
	man1/gpasswd.1 \
	man8/groupadd.8 \
	man8/groupdel.8 \
	man8/groupmems.8 \
	man8/groupmod.8 \
	man1/groups.1 \
	man8/grpck.8 \
	man8/grpconv.8 \
	man8/grpunconv.8 \
	man5/gshadow.5 \
	man1/login.1 \
	man5/login.defs.5 \
	man8/logoutd.8 \
	man1/newgrp.1 \
	man8/newusers.8 \
	man8/nologin.8 \
	man1/passwd.1 \
	man5/passwd.5 \
	man8/pwck.8 \
	man8/pwconv.8 \
	man8/pwunconv.8 \
	man1/sg.1 \
	man3/shadow.3 \
	man5/shadow.5 \
	man1/su.1 \
	man5/suauth.5 \
	man8/useradd.8 \
	man8/userdel.8 \
	man8/usermod.8 \
	man8/vigr.8 \
	man8/vipw.8

if ENABLE_LASTLOG
man_MANS += man8/lastlog.8
endif

man_nopam = \
	man5/limits.5 \
	man5/login.access.5 \
	man5/porttime.5

if !USE_PAM
man_MANS += $(man_nopam)
endif

man_subids = \
	man1/newgidmap.1 \
	man1/newuidmap.1 \
	man5/subgid.5 \
	man5/subuid.5

if ENABLE_SUBIDS
man_MANS += $(man_subids)
endif

EXTRA_DIST = \
	$(man_MANS) \
	man1/id.1 \
	man8/sulogin.8

if USE_PAM
EXTRA_DIST += $(man_nopam)
endif

if !ENABLE_SUBIDS
EXTRA_DIST += $(man_subids)
endif

include ../generate_translations.mak

