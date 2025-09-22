#! /bin/sh
# Copyright (C) 2022 Google LLC
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

testfiles testfile-dwfl-report-elf-align-shlib.so
testfiles testarchive64.a

# echo "int _start(void) { return 0; }" > test.c
# gcc test.c -nostdlib -static -o test.o
# ar -r test-ar-duplicates.a test.o test.o test.o
# bzip2 -zf test-ar-duplicates.a
testfiles test-ar-duplicates.a

testrun ${abs_builddir}/dwfl-report-offline-memory ./testfile-dwfl-report-elf-align-shlib.so 1
testrun ${abs_builddir}/dwfl-report-offline-memory ./testarchive64.a 3
testrun ${abs_builddir}/dwfl-report-offline-memory ./test-ar-duplicates.a 1

exit 0
