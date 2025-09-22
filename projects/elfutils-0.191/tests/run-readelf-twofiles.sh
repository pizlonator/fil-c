#! /bin/sh
# Copyright (C) 2011, 2018 Red Hat, Inc.
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

testfiles testfile14

testrun >/dev/null ${abs_top_builddir}/src/readelf -w testfile14 testfile14

testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=loc testfile14 testfile14 << EOF

testfile14:


DWARF section [33] '.debug_loc' at offset 0xca9:

 CU [     b] base: 0x0000000000400468 <caller>
 [     0] range 34, 35
          0x000000000040049c <main>..
          0x000000000040049c <main>
           [ 0] breg7 -8
          range 35, 46
          0x000000000040049d <main+0x1>..
          0x00000000004004ad <main+0x11>
           [ 0] breg7 0
          range 46, 47
          0x00000000004004ae <main+0x12>..
          0x00000000004004ae <main+0x12>
           [ 0] breg7 -8

testfile14:


DWARF section [33] '.debug_loc' at offset 0xca9:

 CU [     b] base: 0x0000000000400468 <caller>
 [     0] range 34, 35
          0x000000000040049c <main>..
          0x000000000040049c <main>
           [ 0] breg7 -8
          range 35, 46
          0x000000000040049d <main+0x1>..
          0x00000000004004ad <main+0x11>
           [ 0] breg7 0
          range 46, 47
          0x00000000004004ae <main+0x12>..
          0x00000000004004ae <main+0x12>
           [ 0] breg7 -8
EOF

exit 0
