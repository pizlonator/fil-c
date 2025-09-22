
mandir = @mandir@/cs

man_MANS = \
	man1/expiry.1 \
	man5/faillog.5 \
	man8/faillog.8 \
	man1/gpasswd.1 \
	man8/groupadd.8 \
	man8/groupdel.8 \
	man8/groupmod.8 \
	man1/groups.1 \
	man8/grpck.8 \
	man5/gshadow.5 \
	man8/nologin.8 \
	man5/passwd.5 \
	man5/shadow.5 \
	man1/su.1 \
	man8/vipw.8

if ENABLE_LASTLOG
man_MANS += man8/lastlog.8
endif

EXTRA_DIST = $(man_MANS) \
	man1/id.1 \
	man8/groupmems.8 \
	man8/logoutd.8

