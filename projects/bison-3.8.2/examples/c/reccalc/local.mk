## Copyright (C) 2019-2021 Free Software Foundation, Inc.
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <https://www.gnu.org/licenses/>.

reccalcdir = $(docdir)/%D%

## --------- ##
## RecCalc.  ##
## --------- ##

if FLEX_WORKS
  check_PROGRAMS += %D%/reccalc
  TESTS += %D%/reccalc.test
  nodist_%C%_reccalc_SOURCES = %D%/parse.y %D%/scan.h %D%/scan.c
  BUILT_SOURCES += $(nodist_%C%_reccalc_SOURCES)
  # Don't use gnulib's system headers.
  %C%_reccalc_CPPFLAGS = -I$(top_srcdir)/%D% -I$(top_builddir)/%D%
  # Fighting warnings triggered by Flex is just too painful.
  # %C%_reccalc_CFLAGS = $(TEST_CFLAGS)
endif FLEX_WORKS

%D%/parse.c: $(dependencies)

# Tell Make that parse.o depends on scan.h, so that scan.h is built
# before parse.o.  Obfuscate the name of the target, otherwise
# Automake removes its recipe for parse.o and leaves only our
# additional dependency.
DASH = -
%D%/reccalc$(DASH)parse.o: %D%/scan.h
# Tell Make scan.o depends on parse.h, except that Make sees only
# parse.c, not parse.h.  We can't use BUILT_SOURCES to this end, since
# we use the built bison.
%D%/reccalc$(DASH)scan.o: %D%/parse.c

# Likewise, but for Automake before 1.16.
%D%/examples_c_reccalc_reccalc$(DASH)parse.o: %D%/scan.h
%D%/examples_c_reccalc_reccalc$(DASH)scan.o: %D%/parse.c

## See "info automake 'Multiple Outputs'" for this rule.
%D%/scan.c %D%/scan.h: %D%/scan.stamp
## Recover from the removal of $@
	@if test -f $@; then :; else \
	  trap 'rm -rf %D%/scan.lock %D%/scan.stamp' 1 2 13 15; \
## mkdir is a portable test-and-set
	  if mkdir %D%/scan.lock 2>/dev/null; then \
## This code is being executed by the first process.
	    rm -f %D%/scan.stamp; \
	    $(MAKE) $(AM_MAKEFLAGS) %D%/scan.stamp; \
	    result=$$?; rm -rf %D%/scan.lock; exit $$result; \
	  else \
## This code is being executed by the follower processes.
## Wait until the first process is done.
	    while test -d %D%/scan.lock; do sleep 1; done; \
## Succeed if and only if the first process succeeded.
	    test -f %D%/scan.stamp; \
	  fi; \
	fi

%D%/scan.stamp: %D%/scan.l
	$(AM_V_LEX)rm -f $@ $@.tmp
	$(AM_V_at)$(MKDIR_P) %D%
	$(AM_V_at)touch $@.tmp
## --header introduced in 2.5.6, renamed as --header-file in 2.6.4.
## Backward compatibility ensured since --header is an unambiguous prefix.
	$(AM_V_at)$(LEX) $(AM_LFLAGS) $(LFLAGS) -o%D%/scan.c --header=%D%/scan.h $(srcdir)/%D%/scan.l
	$(AM_V_at)mv $@.tmp $@


EXTRA_DIST += %D%/reccalc.test %D%/scan.l
dist_reccalc_DATA = %D%/parse.y %D%/scan.l %D%/Makefile %D%/README.md
CLEANFILES += %D%/parse.[ch] %D%/parse.output %D%/scan.[ch] %D%/*.stamp
CLEANDIRS += %D%/*.dSYM
