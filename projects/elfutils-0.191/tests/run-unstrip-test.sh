#! /bin/sh
# Copyright (C) 2007-2010 Red Hat, Inc.
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

original=${original:-testfile12}
stripped=${stripped:-testfile17}
debugfile=${debugfile:-${stripped}.debug}

testfiles $original $stripped $debugfile
tempfiles testfile.unstrip testfile.inplace

# These are old reference output from run-test-strip6.sh, when
# strip left the .debug file with unchanged sh_size in
# stripped sections that shrank in the stripped file.  strip
# no longer does that, but unstrip must still handle it.

testrun ${abs_top_builddir}/src/unstrip -o testfile.unstrip $stripped $debugfile

testrun ${abs_top_builddir}/src/elfcmp --hash-inexact $original testfile.unstrip

tempfiles syms-orig syms-testfile

# Check whether relocated symbols changed.
testrun ${abs_top_builddir}/tests/elf-print-reloc-syms $original > syms-orig
testrun ${abs_top_builddir}/tests/elf-print-reloc-syms testfile.unstrip > syms-testfile

testrun diff syms-orig syms-testfile

# Also test modifying the file in place.

rm -f testfile.inplace
cp $debugfile testfile.inplace
chmod 644 testfile.inplace
testrun ${abs_top_builddir}/src/unstrip $stripped testfile.inplace

testrun ${abs_top_builddir}/src/elfcmp --hash-inexact $original testfile.inplace
