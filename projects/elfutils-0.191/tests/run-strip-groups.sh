#! /bin/sh
# Copyright (C) 2011 Red Hat, Inc.
# This file is part of elfutils.
#
# This file is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# elfutils is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# g++ -gdwarf-4 -c testfile58.cxx
# class ct
# {
#   private:
#   int i;
#
#   public:
#   void foo ()
#   {
#     i = 1;
#   }
#
#   int bar ()
#   {
#     return i;
#   }
# };
#
# int baz ()
# {
#   class ct c;
#   c.foo ();
#   return c.bar ();
# }

. $srcdir/test-subr.sh

infile=testfile58
outfile=$infile.stripped
dbgfile=$infile.debug

testfiles $infile
tempfiles $outfile $dbgfile

testrun ${abs_top_builddir}/src/strip -o $outfile -f $dbgfile $infile
testrun ${abs_top_builddir}/src/elflint -q $infile
testrun ${abs_top_builddir}/src/elflint -q $outfile
testrun ${abs_top_builddir}/src/elflint -q -d $dbgfile
