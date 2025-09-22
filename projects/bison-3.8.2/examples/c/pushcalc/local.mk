## Copyright (C) 2020-2021 Free Software Foundation, Inc.
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

pushcalcdir = $(docdir)/%D%

## ------ ##
## Calc.  ##
## ------ ##

check_PROGRAMS += %D%/calc
TESTS += %D%/calc.test
EXTRA_DIST += %D%/calc.test
nodist_%C%_calc_SOURCES = %D%/calc.y
%D%/calc.c: $(dependencies)

# Don't use gnulib's system headers.
%C%_calc_CPPFLAGS = -I$(top_srcdir)/%D% -I$(top_builddir)/%D%
%C%_calc_CFLAGS = $(TEST_CFLAGS)

dist_pushcalc_DATA = %D%/calc.y %D%/Makefile %D%/README.md
CLEANFILES += %D%/calc.[ch] %D%/calc.output
CLEANDIRS += %D%/*.dSYM
