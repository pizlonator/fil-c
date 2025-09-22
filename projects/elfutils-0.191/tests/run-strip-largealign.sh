#! /bin/sh
# Copyright (C) 2021 Runsafe Security, Inc.
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

. $srcdir/test-subr.sh

# = testfile-largealign.S =
# section .data
# align 4096
# dd 0x12345678
#
# nasm -f elf64 -o testfile-largealign.o testfile-largealign.S

infile=testfile-largealign.o
outfile=$infile.stripped

testfiles $infile
tempfiles $outfile

testrun ${abs_top_builddir}/src/strip -o $outfile $infile
testrun ${abs_top_builddir}/src/elflint --gnu $outfile
