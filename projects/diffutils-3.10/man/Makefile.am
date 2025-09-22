# Automakefile for GNU diffutils man pages

# Copyright (C) 2002, 2009-2013, 2015-2023 Free Software Foundation, Inc.

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

dist_man1_MANS = cmp.1 diff.1 diff3.1 sdiff.1
EXTRA_DIST = $(dist_man1_MANS:%.1=%.x) help2man
MAINTAINERCLEANFILES = $(dist_man1_MANS)

S = $(top_srcdir)/src
cmp.1: $S/cmp.c cmp.x
diff.1: $S/diff.c diff.x
diff3.1: $S/diff3.c diff3.x
sdiff.1: $S/sdiff.c sdiff.x

# Directory in which just-built programs reside.  It is used
# to ensure help2man invokes them via the use of PATH below.
bin_dir = ../src

# Depend on the former to get version number changes.
$(dist_man1_MANS): $(SRC_VERSION_C) help2man
	$(AM_V_GEN)base=`expr $@ : '\(.*\).1'`				\
	  && test -x $(bin_dir)/$$base					\
	  && (echo '[NAME]'						\
              && sed 's@/\* *@@; s/-/\\-/;s/^GNU //; q' $S/$$base.c)	\
	     | PATH="$(bin_dir)$(PATH_SEPARATOR)$$PATH"			\
	       $(srcdir)/help2man -i - -i $(srcdir)/$$base.x		\
		 -S '$(PACKAGE) $(VERSION)' $$base > $@-t && mv $@-t $@
