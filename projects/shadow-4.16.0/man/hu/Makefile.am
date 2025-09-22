
mandir = @mandir@/hu

man_MANS = \
	man1/chsh.1 \
	man1/gpasswd.1 \
	man1/groups.1 \
	man1/login.1 \
	man1/newgrp.1 \
	man1/passwd.1 \
	man5/passwd.5 \
	man1/sg.1 \
	man1/su.1

if ENABLE_LASTLOG
man_MANS += man8/lastlog.8
endif

EXTRA_DIST = $(man_MANS)
 
