#! /bin/sh
# Copyright (C) 2018 Red Hat, Inc.
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

# Generated on s390x with an older gas (2.29.1) that generates
# badly aligned version notes.
#
# = testfile-version.s =
#
#	.section ".extra"
#	.byte 42
#
#	.version "Sliding Snow"
#	.version "Hurr durr 3.1"
#
#        .globl  _start
#_start:
#
# gcc -nostartfiles -nodefaultlibs -o testfile-version testfile-version.s

testfiles testfile-version
tempfiles debug.out elf.out

testrun ${abs_top_builddir}/src/strip -o elf.out -f debug.out \
	testfile-version

testrun ${abs_top_builddir}/src/elflint --gnu elf.out
testrun ${abs_top_builddir}/src/elflint --gnu --debug debug.out

testrun_compare ${abs_top_builddir}/src/readelf -n debug.out <<\EOF

Note section [ 1] '.note.gnu.build-id' of 36 bytes at offset 0xb0:
  Owner          Data size  Type
  GNU                   20  GNU_BUILD_ID
    Build ID: d3c84c0b307c06f50a37c6c0f59c82c4cb10720b

Note section [ 3] '.note' of 56 bytes at offset 0xd5:
  Owner          Data size  Type
  Sliding Snow           0  VERSION
  Hurr durr 3.1          0  VERSION
EOF

exit 0
