AM_CPPFLAGS = \
	-I$(srcdir)/.. -I$(builddir)/.. \
	-I$(srcdir)/../libcommon -I$(builddir)/../libcommon \
	-D_GNU_SOURCE -DLOCALEDIR=\"$(localedir)\"

EXTRA_DIST = README.vlock vlock.pamd

if VLOCK
bin_PROGRAMS = vlock

install-data-local:
	install -Dm644 $(srcdir)/vlock.pamd $(DESTDIR)$(sysconfdir)/pam.d/vlock
uninstall-local:
	rm $(DESTDIR)$(sysconfdir)/pam.d/vlock
endif

vlock_SOURCES  = \
	auth.c \
	pam_auth.h \
	pam.c \
	parse.c \
	screen.c \
	username.c \
	vlock.c \
	vlock.h \
	vt.c

vlock_LDADD = $(top_builddir)/src/libcommon/libcommon.a @PAM_LIBS@
