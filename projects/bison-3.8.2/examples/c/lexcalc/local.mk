## Copyright (C) 2018-2021 Free Software Foundation, Inc.
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

lexcalcdir = $(docdir)/%D%

## --------- ##
## LexCalc.  ##
## --------- ##

if FLEX_WORKS
  check_PROGRAMS += %D%/lexcalc
  TESTS += %D%/lexcalc.test
  nodist_%C%_lexcalc_SOURCES = %D%/parse.y %D%/parse.h %D%/scan.l
  # Don't use gnulib's system headers.
  %C%_lexcalc_CPPFLAGS = -I$(top_srcdir)/%D% -I$(top_builddir)/%D%
  # Fighting warnings triggered by Flex is just too painful.
  # %C%_lexcalc_CFLAGS = $(TEST_CFLAGS)
endif FLEX_WORKS

%D%/parse.c: $(dependencies)

# Tell Make scan.o depends on parse.h, except that Make sees only
# parse.c, not parse.h.  We can't use BUILT_SOURCES to this end, since
# we use the built bison.
%D%/lexcalc$(DASH)scan.o: %D%/parse.c
# Likewise, but for Automake before 1.16.
%D%/examples_c_lexcalc_lexcalc$(DASH)scan.o: %D%/parse.c

EXTRA_DIST += %D%/lexcalc.test
dist_lexcalc_DATA = %D%/parse.y %D%/scan.l %D%/Makefile %D%/README.md
CLEANFILES += %D%/parse.[ch] %D%/scan.c %D%/parse.output
CLEANDIRS += %D%/*.dSYM
