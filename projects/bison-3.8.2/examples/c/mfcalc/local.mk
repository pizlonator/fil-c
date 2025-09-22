## Copyright (C) 2005-2006, 2008-2015, 2018-2021 Free Software
## Foundation, Inc.
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

## -------------------- ##
## Building & testing.  ##
## -------------------- ##

BUILT_SOURCES += $(mfcalc_sources)
CLEANFILES +=  %D%/mfcalc.[ch] %D%/mfcalc.output
CLEANDIRS += %D%/*.dSYM

mfcalc_extracted = %D%/calc.h %D%/mfcalc.y
mfcalc_sources = $(mfcalc_extracted)
extracted += $(mfcalc_extracted)

check_PROGRAMS += %D%/mfcalc
nodist_%C%_mfcalc_SOURCES = $(mfcalc_sources)
%D%/mfcalc.c: $(dependencies)
# Don't use gnulib's system headers.
%C%_mfcalc_CPPFLAGS = -I$(top_srcdir)/%D% -I$(top_builddir)/%D%
%C%_mfcalc_CFLAGS = $(TEST_CFLAGS)
%C%_mfcalc_LDADD = -lm

dist_TESTS += %D%/mfcalc.test

## ------------ ##
## Installing.  ##
## ------------ ##

mfcalcdir = $(docdir)/%D%
mfcalc_DATA = $(mfcalc_extracted)
dist_mfcalc_DATA = %D%/Makefile
