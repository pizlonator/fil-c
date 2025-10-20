#
# Copyright (c) 2005, 2006, 2007 Thorsten Kukuk <kukuk@thkukuk.de>
#

AUTOMAKE_OPTIONS = 1.9 gnu dist-xz no-dist-gzip check-news

SUBDIRS = libpam_internal libpam tests libpamc libpam_misc modules po conf \
	  xtests

if HAVE_DOC
SUBDIRS += doc
endif
if HAVE_EXAMPLES
SUBDIRS += examples
endif

CLEANFILES = *~

EXTRA_DIST = pgp.keys.asc CHANGELOG ChangeLog-CVS Copyright

ACLOCAL_AMFLAGS = -I m4

release: dist releasedocs

release-docs: releasedocs

releasedocs:
	rm -rf Linux-PAM-$(VERSION)
	mkdir -p Linux-PAM-$(VERSION)/doc
	make -C doc releasedocs
	tar Jfc Linux-PAM-$(VERSION)-docs.tar.xz \
                Linux-PAM-$(VERSION)/doc
	rm -rf Linux-PAM-$(VERSION)

xtests:
	make -C xtests xtests

.PHONY: xtests

gen_changelog_start_date = 2011-10-26
gen-ChangeLog:
	if test -d .git; then						\
		( $(top_srcdir)/gitlog-to-changelog --append-dot	\
		  --since=$(gen_changelog_start_date) &&		\
		  echo && echo &&					\
		  echo 'See ChangeLog-CVS for earlier changes.'		\
		) > $(distdir)/ChangeLog.new &&				\
		rm -f $(distdir)/ChangeLog &&				\
		mv $(distdir)/ChangeLog.new $(distdir)/ChangeLog;	\
	fi

dist-hook: gen-ChangeLog
.PHONY: gen-ChangeLog

export TAR_OPTIONS = --owner=0 --group=0 --numeric-owner --mode=go-w,go+rX
