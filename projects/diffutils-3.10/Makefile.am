# Main Automakefile for GNU diffutils.

# Copyright (C) 2001-2002, 2004, 2006, 2009-2013, 2015-2023 Free Software
# Foundation, Inc.

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

ALL_RECURSIVE_TARGETS =

EXTRA_DIST = bootstrap exgettext ChangeLog-2008 cfg.mk dist-check.mk
SUBDIRS = lib src tests doc man po gnulib-tests

ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = $(WARN_CFLAGS) $(WERROR_CFLAGS)

# Arrange so that .tarball-version appears only in the distribution
# tarball, and never in a checked-out repository.
dist-hook: gen-ChangeLog
	$(AM_V_GEN)echo $(VERSION) > $(distdir)/.tarball-version

gen_start_date = '2009-11-11 15:00'
.PHONY: gen-ChangeLog
gen-ChangeLog:
	$(AM_V_GEN)if test -d .git; then				\
	  $(top_srcdir)/build-aux/gitlog-to-changelog			\
	    --since=$(gen_start_date) > $(distdir)/cl-t &&		\
	    { rm -f $(distdir)/ChangeLog &&				\
	      mv $(distdir)/cl-t $(distdir)/ChangeLog; }		\
	fi

ALL_RECURSIVE_TARGETS += distcheck-hook
distcheck-hook:
	$(MAKE) my-distcheck

# Some of our git hook scripts are supposed to be identical to git's samples.
# See if they are still in sync.
.PHONY: check-git-hook-script-sync
check-git-hook-script-sync:
	@fail=0;							\
	t=$$(mktemp -d)							\
	  && cd $$t && git init -q && cd .git/hooks			\
	  && for i in pre-commit pre-applypatch applypatch-msg; do	\
	       diff -u $(abs_top_srcdir)/scripts/git-hooks/$$i $$i.sample	\
		 || fail=1;						\
	     done;							\
	rm -rf $$t;							\
	test $$fail = 0
