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

java_calcdir = $(docdir)/%D%

## ------ ##
## Calc.  ##
## ------ ##

if ENABLE_JAVA
  check_SCRIPTS += %D%/Calc.class
  TESTS += %D%/Calc.test
endif
EXTRA_DIST += %D%/Calc.test

%D%/Calc.java: %D%/Calc.y $(dependencies)
	$(AM_V_GEN)$(MKDIR_P) %D%
	$(AM_V_at)$(BISON) -o $@ $(srcdir)/%D%/Calc.y

%D%/Calc.class: %D%/Calc.java
	$(AM_V_GEN) $(SHELL) $(top_builddir)/javacomp.sh %D%/Calc.java

dist_java_calc_DATA = %D%/Calc.y %D%/Makefile
CLEANFILES += %D%/*.class %D%/Calc.java
