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

# See tests/testfile-dwarf-45.source
testfiles testfile-splitdwarf-4 testfile-splitdwarf-5

# DWARF4 GNU DebugFission No real table header.
testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=addr testfile-splitdwarf-4<<\EOF

DWARF section [33] '.debug_addr' at offset 0x3671:
Table at offset 0 for CU [     b]:

 Length:              152
 DWARF version:         4
 Address size:          8
 Segment size:          0

 Addresses start at offset 0x0:
 [ 0] 0x000000000040116a <foo+0xa>
 [ 1] 0x0000000000401189 <foo+0x29>
 [ 2] 0x000000000040118d <foo+0x2d>
 [ 3] 0x0000000000401194
 [ 4] 0x0000000000401181 <foo+0x21>
 [ 5] 0x00000000004011af <baz+0xf>
 [ 6] 0x00000000004011b1
 [ 7] 0x00000000004011a0 <baz>
 [ 8] 0x0000000000401160 <foo>
 [ 9] 0x00000000004011a0 <baz>
 [10] 0x000000000040117b <foo+0x1b>
 [11] 0x000000000040117b <foo+0x1b>
 [12] 0x0000000000401181 <foo+0x21>
 [13] 0x0000000000401181 <foo+0x21>
 [14] 0x000000000040118d <foo+0x2d>
 [15] 0x0000000000401160 <foo>
 [16] 0x0000000000401060 <main>
 [17] 0x000000000040117b <foo+0x1b>
 [18] 0x0000000000404038 <m>

Table at offset 98 for CU [    3f]:

 Length:              136
 DWARF version:         4
 Address size:          8
 Segment size:          0

 Addresses start at offset 0x98:
 [ 0] 0x00000000004011df <calc+0x1f>
 [ 1] 0x00000000004011e4 <calc+0x24>
 [ 2] 0x0000000000401060 <main>
 [ 3] 0x0000000000401071 <main+0x11>
 [ 4] 0x0000000000401074 <main+0x14>
 [ 5] 0x0000000000401079 <main+0x19>
 [ 6] 0x00000000004011d3 <calc+0x13>
 [ 7] 0x0000000000401078 <main+0x18>
 [ 8] 0x00000000004011a0 <baz>
 [ 9] 0x0000000000401040
 [10] 0x0000000000401080 <_start>
 [11] 0x00000000004011c0 <calc>
 [12] 0x0000000000401060 <main>
 [13] 0x00000000004011c0 <calc>
 [14] 0x00000000004011c8 <calc+0x8>
 [15] 0x00000000004011d8 <calc+0x18>
 [16] 0x00000000004011da <calc+0x1a>

EOF

# DWARF5 Real table header.
testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=addr testfile-splitdwarf-5<<\EOF

DWARF section [32] '.debug_addr' at offset 0x365e:
Table at offset 0 for CU [    14]:

 Length:              156
 DWARF version:         5
 Address size:          8
 Segment size:          0

 Addresses start at offset 0x8:
 [ 0] 0x000000000040116a <foo+0xa>
 [ 1] 0x0000000000401189 <foo+0x29>
 [ 2] 0x000000000040118d <foo+0x2d>
 [ 3] 0x0000000000401194
 [ 4] 0x0000000000401181 <foo+0x21>
 [ 5] 0x00000000004011af <baz+0xf>
 [ 6] 0x00000000004011b1
 [ 7] 0x00000000004011a0 <baz>
 [ 8] 0x0000000000401160 <foo>
 [ 9] 0x00000000004011a0 <baz>
 [10] 0x000000000040117b <foo+0x1b>
 [11] 0x000000000040117b <foo+0x1b>
 [12] 0x0000000000401181 <foo+0x21>
 [13] 0x0000000000401181 <foo+0x21>
 [14] 0x000000000040118d <foo+0x2d>
 [15] 0x0000000000401160 <foo>
 [16] 0x0000000000401060 <main>
 [17] 0x000000000040117b <foo+0x1b>
 [18] 0x0000000000404038 <m>

Table at offset a0 for CU [    49]:

 Length:              140
 DWARF version:         5
 Address size:          8
 Segment size:          0

 Addresses start at offset 0xa8:
 [ 0] 0x00000000004011df <calc+0x1f>
 [ 1] 0x00000000004011e4 <calc+0x24>
 [ 2] 0x0000000000401060 <main>
 [ 3] 0x0000000000401071 <main+0x11>
 [ 4] 0x0000000000401074 <main+0x14>
 [ 5] 0x0000000000401079 <main+0x19>
 [ 6] 0x00000000004011d3 <calc+0x13>
 [ 7] 0x0000000000401078 <main+0x18>
 [ 8] 0x00000000004011a0 <baz>
 [ 9] 0x0000000000401040
 [10] 0x0000000000401080 <_start>
 [11] 0x00000000004011c0 <calc>
 [12] 0x0000000000401060 <main>
 [13] 0x00000000004011c0 <calc>
 [14] 0x00000000004011c8 <calc+0x8>
 [15] 0x00000000004011d8 <calc+0x18>
 [16] 0x00000000004011da <calc+0x1a>

EOF

exit 0
