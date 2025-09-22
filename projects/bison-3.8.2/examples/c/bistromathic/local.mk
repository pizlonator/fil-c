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

bistromathicdir = $(docdir)/%D%

## --------------- ##
## Bistromathics.  ##
## --------------- ##

%D%/parse.c: $(dependencies)

if ENABLE_BISTROMATHIC
  check_PROGRAMS += %D%/bistromathic
  TESTS += %D%/bistromathic.test
  nodist_%C%_bistromathic_SOURCES = %D%/parse.y

  # Don't use gnulib's system headers.
  %C%_bistromathic_CPPFLAGS =			\
    -DBISON_LOCALEDIR='"$(localdir)"'		\
    -DLOCALEDIR='"$(localdir)"'			\
    -I$(top_srcdir)/%D% -I$(top_builddir)/%D%
  %C%_bistromathic_CFLAGS = $(TEST_CFLAGS)
  %C%_bistromathic_LDADD = -lm $(LIBREADLINE) $(LIBINTL)
endif

EXTRA_DIST += %D%/bistromathic.test
dist_bistromathic_DATA = %D%/parse.y %D%/Makefile %D%/README.md
CLEANFILES += %D%/parse.[ch] %D%/parse.output
CLEANDIRS += %D%/*.dSYM
