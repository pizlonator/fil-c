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

# Merge string tables of file and check result with elflint.
testrun_elfcompress()
{
    testfile="$1"
    testfiles ${testfile}

    mergedfile="${testfile}.merged"
    tempfiles ${mergedfile}

    echo "merging string tables ${testfile} -> ${mergedfile}"
    testrun ${abs_top_builddir}/tests/elfstrmerge -o ${mergedfile} ${testfile}
    testrun ${abs_top_builddir}/src/elflint --gnu-ld ${mergedfile}
}

# Random ELF32 testfile with extra STT_SECTION symbols
testrun_elfcompress testfile4

# Random ELF64 testfile with extra STT_SECTION symbols
testrun_elfcompress testfile12

exit 0
