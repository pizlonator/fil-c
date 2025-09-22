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

# See run-readelf-loc.sh

testfiles testfileloc

# Process values as offsets from base addresses and resolve to symbols.
testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=ranges testfileloc<<\EOF

DWARF section [34] '.debug_ranges' at offset 0xd94:

 CU [     b] base: 0x0000000000400480 <main>
 [     0] range 0, 2
          0x0000000000400480 <main>..
          0x0000000000400481 <main+0x1>
          range 5, d
          0x0000000000400485 <main+0x5>..
          0x000000000040048c <main+0xc>

 CU [    e0] base: 0x00000000004004a0 <say>
 [    30] range d, f
          0x00000000004004ad <say+0xd>..
          0x00000000004004ae <say+0xe>
          range 12, 1a
          0x00000000004004b2 <say+0x12>..
          0x00000000004004b9 <say+0x19>
EOF

# Don't resolve addresses to symbols.
testrun_compare ${abs_top_builddir}/src/readelf -N --debug-dump=ranges testfileloc<<\EOF

DWARF section [34] '.debug_ranges' at offset 0xd94:

 CU [     b] base: 0x0000000000400480
 [     0] range 0, 2
          0x0000000000400480..
          0x0000000000400481
          range 5, d
          0x0000000000400485..
          0x000000000040048c

 CU [    e0] base: 0x00000000004004a0
 [    30] range d, f
          0x00000000004004ad..
          0x00000000004004ae
          range 12, 1a
          0x00000000004004b2..
          0x00000000004004b9
EOF

# Produce "raw" unprocessed content.
testrun_compare ${abs_top_builddir}/src/readelf -U --debug-dump=ranges testfileloc<<\EOF

DWARF section [34] '.debug_ranges' at offset 0xd94:

 CU [     b] base: 0x0000000000400480
 [     0] range 0, 2
          range 5, d

 CU [    e0] base: 0x00000000004004a0
 [    30] range d, f
          range 12, 1a
EOF

# .debug_rnglists (DWARF5), see tests/testfile-dwarf-45.source
testfiles testfile-dwarf-5
testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=ranges testfile-dwarf-5<<\EOF

DWARF section [33] '.debug_rnglists' at offset 0x1d9a:
Table at Offset 0x0:

 Length:               45
 DWARF version:         5
 Address size:          8
 Segment size:          0
 Offset entries:        0
 CU [   218] base: 000000000000000000

  Offset: c, Index: 0
    base_address 0x400583
      0x0000000000400583 <calc+0x13>
    offset_pair 0, 2
      0x0000000000400583 <calc+0x13>..
      0x0000000000400584 <calc+0x14>
    offset_pair 5, 15
      0x0000000000400588 <calc+0x18>..
      0x0000000000400597 <calc+0x27>
    end_of_list

  Offset: 1c, Index: 10
    start_length 0x400570, 2b
      0x0000000000400570 <calc>..
      0x000000000040059a <calc+0x2a>
    start_length 0x400410, 20
      0x0000000000400410 <main>..
      0x000000000040042f <main+0x1f>
    end_of_list

EOF

# Same as above, but for DWARF4, note no header, and base address is not
# given, but ranges are the same.
testfiles testfile-dwarf-4
testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=ranges testfile-dwarf-4<<\EOF

DWARF section [32] '.debug_ranges' at offset 0x1f96:

 CU [   21c] base: 000000000000000000
 [     0] range 400583, 400585
          0x0000000000400583 <calc+0x13>..
          0x0000000000400584 <calc+0x14>
          range 400588, 400598
          0x0000000000400588 <calc+0x18>..
          0x0000000000400597 <calc+0x27>
 [    30] range 400570, 40059b
          0x0000000000400570 <calc>..
          0x000000000040059a <calc+0x2a>
          range 400410, 400430
          0x0000000000400410 <main>..
          0x000000000040042f <main+0x1f>
EOF

# Now with split dwarf. See tests/testfile-dwarf-45.source.
# Note that this will have an offsets table that the .dwo can refer to.
testfiles testfile-splitdwarf-5 testfile-hello5.dwo testfile-world5.dwo
testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=ranges testfile-splitdwarf-5<<\EOF

DWARF section [35] '.debug_rnglists' at offset 0x393a:
Table at Offset 0x0:

 Length:               53
 DWARF version:         5
 Address size:          8
 Segment size:          0
 Offset entries:        2
 CU [    49] base: 000000000000000000

  Offsets starting at 0xc:
   [     0] 0x8
   [     1] 0x18

  Offset: 14, Index: 8
    base_address 0x4011d3
      0x00000000004011d3 <calc+0x13>
    offset_pair 0, 2
      0x00000000004011d3 <calc+0x13>..
      0x00000000004011d4 <calc+0x14>
    offset_pair 5, 15
      0x00000000004011d8 <calc+0x18>..
      0x00000000004011e7 <calc+0x27>
    end_of_list

  Offset: 24, Index: 18
    start_length 0x4011c0, 2b
      0x00000000004011c0 <calc>..
      0x00000000004011ea <calc+0x2a>
    start_length 0x401060, 20
      0x0000000000401060 <main>..
      0x000000000040107f <main+0x1f>
    end_of_list

EOF

# Note that the rnglist_base attribute of the second CU points to the offsets
# above 0xc [c].
testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=info testfile-splitdwarf-5<<\EOF

DWARF section [28] '.debug_info' at offset 0x3102:
 [Offset]
 Compilation unit at offset 0:
 Version: 5, Abbreviation section offset: 0, Address size: 8, Offset size: 4
 Unit type: skeleton (4), Unit id: 0xc422aa5c31fec205
 [    14]  skeleton_unit        abbrev: 1
           low_pc               (addr) 0x0000000000401160 <foo>
           high_pc              (data8) 81 (0x00000000004011b1)
           stmt_list            (sec_offset) 0
           dwo_name             (strp) "testfile-hello5.dwo"
           comp_dir             (strp) "/home/mark/src/elfutils/tests"
           GNU_pubnames         (flag_present) yes
           addr_base            (sec_offset) address base [     8]
 Compilation unit at offset 53:
 Version: 5, Abbreviation section offset: 21, Address size: 8, Offset size: 4
 Unit type: skeleton (4), Unit id: 0xb6c8b9d97e6dfdfe
 [    49]  skeleton_unit        abbrev: 1
           ranges               (sec_offset) range list [    24]
           low_pc               (addr) 000000000000000000
           stmt_list            (sec_offset) 655
           dwo_name             (strp) "testfile-world5.dwo"
           comp_dir             (strp) "/home/mark/src/elfutils/tests"
           GNU_pubnames         (flag_present) yes
           addr_base            (sec_offset) address base [    a8]
           rnglists_base        (sec_offset) range list [     c]
EOF

# Same for DWARF4 GNU DebugFission. But now we need to scan the .dwo
# explicitly to know it will use the first ranges.
testfiles testfile-splitdwarf-4 testfile-hello4.dwo testfile-world4.dwo
testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=ranges testfile-splitdwarf-4<<\EOF

DWARF section [32] '.debug_ranges' at offset 0x3611:

 CU [     b] base: 000000000000000000
 [     0] range 4011d3, 4011d5
          0x00000000004011d3 <calc+0x13>..
          0x00000000004011d4 <calc+0x14>
          range 4011d8, 4011e8
          0x00000000004011d8 <calc+0x18>..
          0x00000000004011e7 <calc+0x27>

 CU [    3f] base: 000000000000000000
 [    30] range 4011c0, 4011eb
          0x00000000004011c0 <calc>..
          0x00000000004011ea <calc+0x2a>
          range 401060, 401080
          0x0000000000401060 <main>..
          0x000000000040107f <main+0x1f>
EOF

exit 0
