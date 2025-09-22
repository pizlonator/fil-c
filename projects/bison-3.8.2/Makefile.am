## Process this file with automake to produce Makefile.in.

# Copyright (C) 2001-2015, 2018-2021 Free Software Foundation, Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = po runtime-po gnulib-po .

# Files installed for use by Automake.
aclocaldir = @aclocaldir@
aclocal_DATA = m4/bison-i18n.m4

EXTRA_DIST = .prev-version .version cfg.mk PACKAGING \
  ChangeLog-1998 ChangeLog-2012 ChangeLog

dist_doc_DATA = AUTHORS COPYING NEWS README THANKS TODO

# Let generated files (e.g., from examples/) depend on this variable,
# to make sure they are regenerated when there are changes in Bison.
#
# Don't depend on $(BISON) otherwise we would rebuild these files
# in srcdir, including during distcheck, which is forbidden.
dependencies = $(BISON_IN) $(src_bison_SOURCES) $(dist_skeletons_DATA)

## Running the bison from this tarball.  To generate our own parser,
## but also to run the tests.  Of course, you ought to keep a sane
## version of Bison nearby...
BISON = $(top_builddir)/tests/bison
BISON_IN = $(top_srcdir)/tests/bison.in
YACC = $(BISON) -o y.tab.c
AM_YFLAGS_WITH_LINES = --defines -Werror -Wall,dangling-alias --report=all
AM_YFLAGS = $(AM_YFLAGS_WITH_LINES) --no-lines

# Initialization before completion by local.mk's.
AM_CFLAGS = $(WARN_CFLAGS)
# Find builddir/src/scan-code.c etc.  For some reason "-I./lib"
# instead of "-Ilib" avoids infinite recursions on #include_next.
AM_CPPFLAGS = -I. -I./lib -I$(top_srcdir) -I$(top_srcdir)/lib
BUILT_SOURCES =
CLEANDIRS =
CLEANFILES =
DISTCLEANFILES =
EXTRA_DIST += $(dist_TESTS)
MOSTLYCLEANDIRS =
MOSTLYCLEANFILES =
SUFFIXES =
TESTS = $(dist_TESTS)
check_PROGRAMS =
check_SCRIPTS =
dist_TESTS =
noinst_LIBRARIES =

include build-aux/local.mk
include data/local.mk
include doc/local.mk
include etc/local.mk
include examples/local.mk
include lib/local.mk
include src/local.mk
include tests/local.mk

clean-local:
	rm -rf $(CLEANDIRS)

.PHONY: gitsort
gitsort:
	cd $(srcdir)							\
	&& for i in $$(git ls-files '*.gitignore' | grep -v doc); do	\
	  LC_ALL=C sort $$i -o $$i;					\
	done

# See comments in build-aux/git-version-gen.  However, we make .version depend
# on configure so that .version and VERSION/PACKAGE_VERSION stay in sync in the
# working copy (for example, when you run autoreconf && make).  Allowing these
# to fall out of sync appears to have little potential to improve Bison build
# efficiency (even if we were to replace VERSION/PACKAGE_VERSION with .version
# everywhere possible).  On the other hand, it could be harmful.  For example,
# a developer might naively reference .version in a test case while the bison
# executable still compiles with VERSION, and so the test case might fail or
# pass incorrectly.
BUILT_SOURCES += $(top_srcdir)/.version
$(top_srcdir)/.version: configure
	echo $(VERSION) > $@-t && mv $@-t $@
dist-hook: gen-ChangeLog gen-synclines
	echo $(VERSION) > $(distdir)/.tarball-version

# When generating a release, include the #lines in Bison's parser.  Do it
# only if needed.  In particular, distcheck runs dist with read-only
# sources, so don't try to update src/parse-gram.c: we don't have the
# permissions.
.PHONY: gen-synclines
gen-synclines:
	cd $(distdir) &&						\
	if ! grep '#line' src/parse-gram.c >/dev/null 2>&1; then	\
	  $(abs_top_builddir)/tests/bison				\
	    $(AM_YFLAGS_WITH_LINES) src/parse-gram.y			\
	    -o src/parse-gram.c;					\
	fi

.PHONY: update-b4-copyright update-package-copyright-year
update-b4-copyright:
	$(AM_V_GEN)find $(srcdir)/data -type f          \
	  | xargs $(srcdir)/build-aux/$@
	@echo 'warning: src/parse-gram.[hc] may need to be regenerated.'
update-package-copyright-year:
	$(AM_V_GEN)$(srcdir)/build-aux/$@ $(srcdir)/configure.ac

gen_start_date = 2012-01-16
.PHONY: gen-ChangeLog
gen-ChangeLog:
	$(AM_V_GEN)if test -d $(srcdir)/.git; then	\
	  cl=$(distdir)/ChangeLog &&			\
	  rm -f $$cl.tmp &&				\
	  $(top_srcdir)/build-aux/gitlog-to-changelog	\
	    --strip-tab					\
	    --strip-cherry-pick				\
	    --no-cluster				\
	    --amend=$(srcdir)/build-aux/git-log-fix	\
	    --srcdir=$(srcdir)				\
	    --since=$(gen_start_date) > $$cl.tmp &&	\
	  mv -f $$cl.tmp $$cl;				\
	fi

# Useful to debug.
.c.i:
	$(CC) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -o $@ -E $<

.PHONY: codespell
CODESPELL = codespell
codespell:
	$(AM_V_GEN) cd $(srcdir) \
	&& $(CODESPELL) \
	     --regex "[\\w\\-'’\`]+\+*" \
	     --ignore-words-list "ba,circularly,cloneable,copyable,define'd,dout,froms,iff,ifset,od,ois" \
	     $$(git ls-files data doc lib src tests)
