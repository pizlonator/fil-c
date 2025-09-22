#! /bin/sh
# Copyright (C) 2015 Red Hat, Inc.
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

. $srcdir/test-subr.sh

# Generate a file with merged .shstrtab/.strtab table.
# strip and unstrip it. Check all files with elflint.

# A random ET_EXEC file
input=${abs_top_builddir}/tests/elfstrmerge
merged=merged.elf
stripped=${merged}.stripped
debugfile=${merged}.debug
remerged=remerged.elf

tempfiles $merged $stripped $debugfile $remerged

echo elflint $input
testrun ${abs_top_builddir}/src/elflint --gnu $input
echo elfstrmerge
testrun ${abs_top_builddir}/tests/elfstrmerge -o $merged $input
echo elflint $merged
testrun ${abs_top_builddir}/src/elflint --gnu $merged
echo strip
testrun ${abs_top_builddir}/src/strip -o $stripped -f $debugfile $merged
echo elflint $stripped
testrun ${abs_top_builddir}/src/elflint --gnu $stripped
echo elflint $debugfile
testrun ${abs_top_builddir}/src/elflint --gnu -d $debugfile
echo unstrip
testrun ${abs_top_builddir}/src/unstrip -o $remerged $stripped $debugfile
echo elflint $remerged
testrun ${abs_top_builddir}/src/elflint --gnu $remerged
echo elfcmp
testrun ${abs_top_builddir}/src/elfcmp $merged $remerged

# A random ET_REL file
input=${abs_top_builddir}/tests/elfstrmerge.o
merged=merged.elf
stripped=${merged}.stripped
debugfile=${merged}.debug
remerged=remerged.elf

tempfiles $merged $stripped $debugfile $remerged

echo elflint $input
testrun ${abs_top_builddir}/src/elflint --gnu $input
echo elfstrmerge
testrun ${abs_top_builddir}/tests/elfstrmerge -o $merged $input
echo elflint $merged
testrun ${abs_top_builddir}/src/elflint --gnu $merged
echo strip
testrun ${abs_top_builddir}/src/strip -o $stripped -f $debugfile $merged
echo elflint $stripped
testrun ${abs_top_builddir}/src/elflint --gnu $stripped
echo elflint $debugfile
testrun ${abs_top_builddir}/src/elflint --gnu -d $debugfile
echo unstrip
testrun ${abs_top_builddir}/src/unstrip -o $remerged $stripped $debugfile
echo elflint $remerged
testrun ${abs_top_builddir}/src/elflint --gnu $remerged
echo elfcmp
testrun ${abs_top_builddir}/src/elfcmp $merged $remerged

exit 0
