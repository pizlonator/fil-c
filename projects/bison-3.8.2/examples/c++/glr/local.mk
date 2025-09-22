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

glrxxdir = $(docdir)/%D%

## ----------- ##
## c++-types.  ##
## ----------- ##

%D%/c++-types.stamp: $(dependencies)
$(nodist_%C%_c___types_SOURCES): %D%/c++-types.stamp
	@test -f $@ || rm -f %D%/c++-types.stamp
	@test -f $@ || $(MAKE) $(AM_MAKEFLAGS) %D%/c++-types.stamp
CLEANFILES +=					\
  $(nodist_%C%_c___types_SOURCES)		\
  %D%/c++-types.stamp				\
  %D%/c++-types.output				\
  %D%/location.hh
CLEANDIRS += %D%/*.dSYM

## -------------------- ##
## Building & testing.  ##
## -------------------- ##

# Avoid using BUILT_SOURCES which is too global.
$(%C%_c___types_OBJECTS): $(cxx_types_sources_generated)

if ENABLE_CXX14
  check_PROGRAMS += %D%/c++-types
  dist_%C%_c___types_SOURCES =			\
    %D%/ast.hh
  nodist_%C%_c___types_SOURCES =		\
    %D%/c++-types.cc				\
    %D%/c++-types.hh
  # Don't use gnulib's system headers.
  %C%_c___types_CPPFLAGS = -I$(top_srcdir)/%D% -I$(top_builddir)/%D%
  %C%_c___types_CXXFLAGS = $(CXX14_CXXFLAGS) $(WARN_CXXFLAGS_TEST)
  TESTS += %D%/c++-types.test
endif ENABLE_CXX14
EXTRA_DIST += %D%/c++-types.yy %D%/c++-types.test
