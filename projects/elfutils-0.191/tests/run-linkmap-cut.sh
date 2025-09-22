#! /usr/bin/env bash
# Copyright (C) 2014 Red Hat, Inc.
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

# The core file has patched:
# * _r_debug.r_map.l_next.l_next (vma 0x3fdf621718, offset 0x7718) = NULL,
#   therefore all libraries after the main executable and vDSO are removed.
# * NT_FILE absolute filenames are relativized to: ./////basename

testfiles linkmap-cut-lib.so linkmap-cut linkmap-cut.core
tempfiles bt
# It may have non-zero exit code with:
# .../elfutils/src/stack: dwfl_thread_getframes tid 3130 at 0x3fdf821d64 in /usr/lib64/libc-2.18.so: no matching address range
testrun ${abs_top_builddir}/src/stack --core=linkmap-cut.core -e linkmap-cut -m >bt || true
cat bt
grep -q '^#0  0x00007f08bc24d681 libfunc - .////////////////////////////////////linkmap-cut-lib\.so$' bt
grep -q '^#1  0x00000000004006b4 main - linkmap-cut$' bt
