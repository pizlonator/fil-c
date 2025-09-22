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

glrdir = $(docdir)/%D%

## ----------- ##
## c++-types.  ##
## ----------- ##

check_PROGRAMS += %D%/c++-types
TESTS += %D%/c++-types.test
EXTRA_DIST += %D%/c++-types.test
nodist_%C%_c___types_SOURCES = %D%/c++-types.y
%D%/c++-types.c: $(dependencies)

# Don't use gnulib's system headers.
%C%_c___types_CPPFLAGS = -I$(top_srcdir)/%D% -I$(top_builddir)/%D%
%C%_c___types_CFLAGS = $(TEST_CFLAGS)

dist_glr_DATA = %D%/c++-types.y %D%/Makefile %D%/README.md
CLEANFILES += %D%/c++-types.[ch] %D%/c++-types.output
CLEANDIRS += %D%/*.dSYM
