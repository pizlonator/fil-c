## Copyright (C) 2001-2015, 2018-2021 Free Software Foundation, Inc.
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

include lib/gnulib.mk

# Non-gnulib sources in Bison's internal library.
lib_libbison_a_SOURCES +=                       \
  lib/get-errno.h                               \
  lib/get-errno.c                               \
  lib/path-join.h                               \
  lib/path-join.c

# The Yacc compatibility library.
if ENABLE_YACC
  lib_LIBRARIES = lib/liby.a
  EXTRA_LIBRARIES = lib/liby.a
  lib_liby_a_SOURCES = lib/main.c lib/yyerror.c
endif
