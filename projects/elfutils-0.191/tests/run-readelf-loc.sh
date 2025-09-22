#! /bin/sh
# Copyright (C) 2013 Red Hat, Inc.
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

# - hello.c
# int say (const char *prefix);
#
# static char *
# subject (char *word, int count)
# {
#   return count > 0 ? word : (word + count);
# }
#
# int
# main (int argc, char **argv)
# {
#    return say (subject (argv[0], argc));
# }
#
# - world.c
# static int
# sad (char c)
# {
#   return c > 0 ? c : c + 1;
# }
#
# static int
# happy (const char *w)
# {
#   return sad (w[1]);
# }
#
# int
# say (const char *prefix)
# {
#   const char *world = "World";
#   return prefix ? sad (prefix[0]) : happy (world);
# }
#
# gcc -g -O2 -c hello.c
# gcc -g -O2 -c world.c
# gcc -g -o testfileloc hello.o world.o

testfiles testfileloc

# Process values as offsets from base addresses and resolve to symbols.
testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=loc --debug-dump=ranges \
  testfileloc<<\EOF

DWARF section [33] '.debug_loc' at offset 0xd2a:

 CU [     b] base: 0x0000000000400480 <main>
 [     0] range 0, d
          0x0000000000400480 <main>..
          0x000000000040048c <main+0xc>
           [ 0] reg5
 [    23] range 5, d
          0x0000000000400485 <main+0x5>..
          0x000000000040048c <main+0xc>
           [ 0] reg5

 CU [    e0] base: 0x00000000004004a0 <say>
 [    46] range 12, 1a
          0x00000000004004b2 <say+0x12>..
          0x00000000004004b9 <say+0x19>
           [ 0] breg5 0

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
testrun_compare ${abs_top_builddir}/src/readelf -N --debug-dump=loc --debug-dump=ranges \
  testfileloc<<\EOF

DWARF section [33] '.debug_loc' at offset 0xd2a:

 CU [     b] base: 0x0000000000400480
 [     0] range 0, d
          0x0000000000400480..
          0x000000000040048c
           [ 0] reg5
 [    23] range 5, d
          0x0000000000400485..
          0x000000000040048c
           [ 0] reg5

 CU [    e0] base: 0x00000000004004a0
 [    46] range 12, 1a
          0x00000000004004b2..
          0x00000000004004b9
           [ 0] breg5 0

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
testrun_compare ${abs_top_builddir}/src/readelf -U --debug-dump=loc --debug-dump=ranges \
  testfileloc<<\EOF

DWARF section [33] '.debug_loc' at offset 0xd2a:

 CU [     b] base: 0x0000000000400480
 [     0] range 0, d
           [ 0] reg5
 [    23] range 5, d
           [ 0] reg5

 CU [    e0] base: 0x00000000004004a0
 [    46] range 12, 1a
           [ 0] breg5 0

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
testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=loc testfile-dwarf-5<<\EOF

DWARF section [31] '.debug_loclists' at offset 0x1c0c:
Table at Offset 0x0:

 Length:               96
 DWARF version:         5
 Address size:          8
 Segment size:          0
 Offset entries:        0
 CU [     c] base: 0x0000000000400510 <foo>

  Offset: c, Index: 0
    offset_pair 0, a
      0x0000000000400510 <foo>..
      0x0000000000400519 <foo+0x9>
        [ 0] reg5
    offset_pair a, 34
      0x000000000040051a <foo+0xa>..
      0x0000000000400543 <foo+0x33>
        [ 0] entry_value:
             [ 0] reg5
        [ 3] stack_value
    end_of_list

  Offset: 1a, Index: e
    offset_pair 1b, 2d
      0x000000000040052b <foo+0x1b>..
      0x000000000040053c <foo+0x2c>
        [ 0] addr 0x601038 <m>
    end_of_list

  Offset: 28, Index: 1c
    offset_pair 1b, 21
      0x000000000040052b <foo+0x1b>..
      0x0000000000400530 <foo+0x20>
        [ 0] reg5
    end_of_list

  Offset: 2e, Index: 22
    offset_pair 1b, 27
      0x000000000040052b <foo+0x1b>..
      0x0000000000400536 <foo+0x26>
        [ 0] reg5
    offset_pair 29, 2d
      0x0000000000400539 <foo+0x29>..
      0x000000000040053c <foo+0x2c>
        [ 0] reg5
    end_of_list

  Offset: 39, Index: 2d
    offset_pair 21, 27
      0x0000000000400531 <foo+0x21>..
      0x0000000000400536 <foo+0x26>
        [ 0] reg5
    offset_pair 29, 2d
      0x0000000000400539 <foo+0x29>..
      0x000000000040053c <foo+0x2c>
        [ 0] reg5
    end_of_list

  Offset: 44, Index: 38
    offset_pair 21, 2d
      0x0000000000400531 <foo+0x21>..
      0x000000000040053c <foo+0x2c>
        [ 0] reg5
    end_of_list

  Offset: 4a, Index: 3e
    offset_pair 2d, 33
      0x000000000040053d <foo+0x2d>..
      0x0000000000400542 <foo+0x32>
        [ 0] reg5
    end_of_list

  Offset: 50, Index: 44
    offset_pair 40, 4f
      0x0000000000400550 <baz>..
      0x000000000040055e <baz+0xe>
        [ 0] reg5
    offset_pair 4f, 51
      0x000000000040055f <baz+0xf>..
      0x0000000000400560 <baz+0x10>
        [ 0] entry_value:
             [ 0] reg5
        [ 3] stack_value
    end_of_list

  Offset: 5e, Index: 52
    offset_pair 40, 50
      0x0000000000400550 <baz>..
      0x000000000040055f <baz+0xf>
        [ 0] reg5
    end_of_list

Table at Offset 0x64:

 Length:              159
 DWARF version:         5
 Address size:          8
 Segment size:          0
 Offset entries:        0
 CU [   218] base: 000000000000000000

  Offset: 70, Index: 0
    base_address 0x400410
      0x0000000000400410 <main>
    offset_pair 0, 14
      0x0000000000400410 <main>..
      0x0000000000400423 <main+0x13>
        [ 0] reg5
    offset_pair 14, 20
      0x0000000000400424 <main+0x14>..
      0x000000000040042f <main+0x1f>
        [ 0] entry_value:
             [ 0] reg5
        [ 3] stack_value
    end_of_list

  Offset: 87, Index: 17
    base_address 0x400410
      0x0000000000400410 <main>
    offset_pair 0, 18
      0x0000000000400410 <main>..
      0x0000000000400427 <main+0x17>
        [ 0] reg4
    offset_pair 18, 20
      0x0000000000400428 <main+0x18>..
      0x000000000040042f <main+0x1f>
        [ 0] entry_value:
             [ 0] reg4
        [ 3] stack_value
    end_of_list

  Offset: 9e, Index: 2e
    start_length 0x400421, 7
      0x0000000000400421 <main+0x11>..
      0x0000000000400427 <main+0x17>
        [ 0] reg0
    end_of_list

  Offset: ab, Index: 3b
    base_address 0x400570
      0x0000000000400570 <calc>
    offset_pair 0, 8
      0x0000000000400570 <calc>..
      0x0000000000400577 <calc+0x7>
        [ 0] reg5
    offset_pair 8, 2b
      0x0000000000400578 <calc+0x8>..
      0x000000000040059a <calc+0x2a>
        [ 0] entry_value:
             [ 0] reg5
        [ 3] stack_value
    end_of_list

  Offset: c2, Index: 52
    start_length 0x400588, b
      0x0000000000400588 <calc+0x18>..
      0x0000000000400592 <calc+0x22>
        [ 0] reg0
    end_of_list

  Offset: cf, Index: 5f
    base_address 0x400588
      0x0000000000400588 <calc+0x18>
    offset_pair 0, 2
      0x0000000000400588 <calc+0x18>..
      0x0000000000400589 <calc+0x19>
        [ 0] reg1
    offset_pair 2, 7
      0x000000000040058a <calc+0x1a>..
      0x000000000040058e <calc+0x1e>
        [ 0] reg5
    offset_pair 7, b
      0x000000000040058f <calc+0x1f>..
      0x0000000000400592 <calc+0x22>
        [ 0] entry_value:
             [ 0] reg5
        [ 3] deref_size 1
        [ 5] const1u 56
        [ 7] shl
        [ 8] const1u 56
        [10] shra
        [11] stack_value
    end_of_list

  Offset: f3, Index: 83
    base_address 0x400588
      0x0000000000400588 <calc+0x18>
    offset_pair 0, 2
      0x0000000000400588 <calc+0x18>..
      0x0000000000400589 <calc+0x19>
        [ 0] reg1
    offset_pair 2, b
      0x000000000040058a <calc+0x1a>..
      0x0000000000400592 <calc+0x22>
        [ 0] reg5
    end_of_list

EOF

# Same as above, but for DWARF4, note completely different encoding, but
# the information is the same (check with diff -uwb).
testfiles testfile-dwarf-4
testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=loc testfile-dwarf-4<<\EOF

DWARF section [31] '.debug_loc' at offset 0x1c86:

 CU [     b] base: 0x0000000000400510 <foo>
 [     0] range 0, a
          0x0000000000400510 <foo>..
          0x0000000000400519 <foo+0x9>
           [ 0] reg5
          range a, 34
          0x000000000040051a <foo+0xa>..
          0x0000000000400543 <foo+0x33>
           [ 0] GNU_entry_value:
                [ 0] reg5
           [ 3] stack_value
 [    39] range 1b, 2d
          0x000000000040052b <foo+0x1b>..
          0x000000000040053c <foo+0x2c>
           [ 0] addr 0x601038 <m>
 [    64] range 1b, 21
          0x000000000040052b <foo+0x1b>..
          0x0000000000400530 <foo+0x20>
           [ 0] reg5
 [    87] range 1b, 27
          0x000000000040052b <foo+0x1b>..
          0x0000000000400536 <foo+0x26>
           [ 0] reg5
          range 29, 2d
          0x0000000000400539 <foo+0x29>..
          0x000000000040053c <foo+0x2c>
           [ 0] reg5
 [    bd] range 21, 27
          0x0000000000400531 <foo+0x21>..
          0x0000000000400536 <foo+0x26>
           [ 0] reg5
          range 29, 2d
          0x0000000000400539 <foo+0x29>..
          0x000000000040053c <foo+0x2c>
           [ 0] reg5
 [    f3] range 21, 2d
          0x0000000000400531 <foo+0x21>..
          0x000000000040053c <foo+0x2c>
           [ 0] reg5
 [   116] range 2d, 33
          0x000000000040053d <foo+0x2d>..
          0x0000000000400542 <foo+0x32>
           [ 0] reg5
 [   139] range 40, 4f
          0x0000000000400550 <baz>..
          0x000000000040055e <baz+0xe>
           [ 0] reg5
          range 4f, 51
          0x000000000040055f <baz+0xf>..
          0x0000000000400560 <baz+0x10>
           [ 0] GNU_entry_value:
                [ 0] reg5
           [ 3] stack_value
 [   172] range 40, 50
          0x0000000000400550 <baz>..
          0x000000000040055f <baz+0xf>
           [ 0] reg5

 CU [   21c] base: 000000000000000000
 [   195] range 400410, 400424
          0x0000000000400410 <main>..
          0x0000000000400423 <main+0x13>
           [ 0] reg5
          range 400424, 400430
          0x0000000000400424 <main+0x14>..
          0x000000000040042f <main+0x1f>
           [ 0] GNU_entry_value:
                [ 0] reg5
           [ 3] stack_value
 [   1ce] range 400410, 400428
          0x0000000000400410 <main>..
          0x0000000000400427 <main+0x17>
           [ 0] reg4
          range 400428, 400430
          0x0000000000400428 <main+0x18>..
          0x000000000040042f <main+0x1f>
           [ 0] GNU_entry_value:
                [ 0] reg4
           [ 3] stack_value
 [   207] range 400421, 400428
          0x0000000000400421 <main+0x11>..
          0x0000000000400427 <main+0x17>
           [ 0] reg0
 [   22a] range 400570, 400578
          0x0000000000400570 <calc>..
          0x0000000000400577 <calc+0x7>
           [ 0] reg5
          range 400578, 40059b
          0x0000000000400578 <calc+0x8>..
          0x000000000040059a <calc+0x2a>
           [ 0] GNU_entry_value:
                [ 0] reg5
           [ 3] stack_value
 [   263] range 400588, 400593
          0x0000000000400588 <calc+0x18>..
          0x0000000000400592 <calc+0x22>
           [ 0] reg0
 [   286] range 400588, 40058a
          0x0000000000400588 <calc+0x18>..
          0x0000000000400589 <calc+0x19>
           [ 0] reg1
          range 40058a, 40058f
          0x000000000040058a <calc+0x1a>..
          0x000000000040058e <calc+0x1e>
           [ 0] reg5
          range 40058f, 400593
          0x000000000040058f <calc+0x1f>..
          0x0000000000400592 <calc+0x22>
           [ 0] GNU_entry_value:
                [ 0] reg5
           [ 3] deref_size 1
           [ 5] const1u 56
           [ 7] shl
           [ 8] const1u 56
           [10] shra
           [11] stack_value
 [   2da] range 400588, 40058a
          0x0000000000400588 <calc+0x18>..
          0x0000000000400589 <calc+0x19>
           [ 0] reg1
          range 40058a, 400593
          0x000000000040058a <calc+0x1a>..
          0x0000000000400592 <calc+0x22>
           [ 0] reg5
EOF

# Split DWARF5 variant. Note that the .debug_loclists moved to the .dwo file
# and now uses an index and addrx indirections.
testfiles testfile-splitdwarf-5 testfile-hello5.dwo testfile-world5.dwo
testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=loc --dwarf-skeleton=testfile-splitdwarf-5 testfile-hello5.dwo testfile-world5.dwo <<\EOF

testfile-hello5.dwo:


DWARF section [ 3] '.debug_loclists.dwo' at offset 0x236:
Table at Offset 0x0:

 Length:              125
 DWARF version:         5
 Address size:          8
 Segment size:          0
 Offset entries:        9
 CU [    14] base: 0x0000000000401160 <foo>

  Offsets starting at 0xc:
   [     0] 0x24
   [     1] 0x32
   [     2] 0x39
   [     3] 0x3f
   [     4] 0x4a
   [     5] 0x55
   [     6] 0x5b
   [     7] 0x61
   [     8] 0x6f

  Offset: 30, Index: 24
    startx_length f, a
      0x0000000000401160 <foo>..
      0x0000000000401169 <foo+0x9>
        [ 0] reg5
    startx_length 0, 2a
      0x000000000040116a <foo+0xa>..
      0x0000000000401193 <foo+0x33>
        [ 0] entry_value:
             [ 0] reg5
        [ 3] stack_value
    end_of_list

  Offset: 3e, Index: 32
    startx_length 11, 12
      0x000000000040117b <foo+0x1b>..
      0x000000000040118c <foo+0x2c>
        [ 0] addrx [18] 0x404038 <m>
    end_of_list

  Offset: 45, Index: 39
    startx_length 11, 6
      0x000000000040117b <foo+0x1b>..
      0x0000000000401180 <foo+0x20>
        [ 0] reg5
    end_of_list

  Offset: 4b, Index: 3f
    startx_length 11, c
      0x000000000040117b <foo+0x1b>..
      0x0000000000401186 <foo+0x26>
        [ 0] reg5
    startx_length 1, 4
      0x0000000000401189 <foo+0x29>..
      0x000000000040118c <foo+0x2c>
        [ 0] reg5
    end_of_list

  Offset: 56, Index: 4a
    startx_length 4, 6
      0x0000000000401181 <foo+0x21>..
      0x0000000000401186 <foo+0x26>
        [ 0] reg5
    startx_length 1, 4
      0x0000000000401189 <foo+0x29>..
      0x000000000040118c <foo+0x2c>
        [ 0] reg5
    end_of_list

  Offset: 61, Index: 55
    startx_length 4, c
      0x0000000000401181 <foo+0x21>..
      0x000000000040118c <foo+0x2c>
        [ 0] reg5
    end_of_list

  Offset: 67, Index: 5b
    startx_length 2, 6
      0x000000000040118d <foo+0x2d>..
      0x0000000000401192 <foo+0x32>
        [ 0] reg5
    end_of_list

  Offset: 6d, Index: 61
    startx_length 9, f
      0x00000000004011a0 <baz>..
      0x00000000004011ae <baz+0xe>
        [ 0] reg5
    startx_length 5, 2
      0x00000000004011af <baz+0xf>..
      0x00000000004011b0 <baz+0x10>
        [ 0] entry_value:
             [ 0] reg5
        [ 3] stack_value
    end_of_list

  Offset: 7b, Index: 6f
    startx_length 9, 10
      0x00000000004011a0 <baz>..
      0x00000000004011af <baz+0xf>
        [ 0] reg5
    end_of_list


testfile-world5.dwo:


DWARF section [ 3] '.debug_loclists.dwo' at offset 0x217:
Table at Offset 0x0:

 Length:              128
 DWARF version:         5
 Address size:          8
 Segment size:          0
 Offset entries:        7
 CU [    14] base: 000000000000000000

  Offsets starting at 0xc:
   [     0] 0x1c
   [     1] 0x2a
   [     2] 0x38
   [     3] 0x3e
   [     4] 0x4c
   [     5] 0x52
   [     6] 0x6d

  Offset: 28, Index: 1c
    startx_length 2, 14
      0x0000000000401060 <main>..
      0x0000000000401073 <main+0x13>
        [ 0] reg5
    startx_length 4, c
      0x0000000000401074 <main+0x14>..
      0x000000000040107f <main+0x1f>
        [ 0] entry_value:
             [ 0] reg5
        [ 3] stack_value
    end_of_list

  Offset: 36, Index: 2a
    startx_length 2, 18
      0x0000000000401060 <main>..
      0x0000000000401077 <main+0x17>
        [ 0] reg4
    startx_length 7, 6
      0x0000000000401078 <main+0x18>..
      0x000000000040107d <main+0x1d>
        [ 0] entry_value:
             [ 0] reg4
        [ 3] stack_value
    end_of_list

  Offset: 44, Index: 38
    startx_length 3, 7
      0x0000000000401071 <main+0x11>..
      0x0000000000401077 <main+0x17>
        [ 0] reg0
    end_of_list

  Offset: 4a, Index: 3e
    startx_length d, 8
      0x00000000004011c0 <calc>..
      0x00000000004011c7 <calc+0x7>
        [ 0] reg5
    startx_length e, 23
      0x00000000004011c8 <calc+0x8>..
      0x00000000004011ea <calc+0x2a>
        [ 0] entry_value:
             [ 0] reg5
        [ 3] stack_value
    end_of_list

  Offset: 58, Index: 4c
    startx_length f, b
      0x00000000004011d8 <calc+0x18>..
      0x00000000004011e2 <calc+0x22>
        [ 0] reg0
    end_of_list

  Offset: 5e, Index: 52
    startx_length f, 2
      0x00000000004011d8 <calc+0x18>..
      0x00000000004011d9 <calc+0x19>
        [ 0] reg1
    startx_length 10, 5
      0x00000000004011da <calc+0x1a>..
      0x00000000004011de <calc+0x1e>
        [ 0] reg5
    startx_length 0, 4
      0x00000000004011df <calc+0x1f>..
      0x00000000004011e2 <calc+0x22>
        [ 0] entry_value:
             [ 0] reg5
        [ 3] deref_size 1
        [ 5] const1u 56
        [ 7] shl
        [ 8] const1u 56
        [10] shra
        [11] stack_value
    end_of_list

  Offset: 79, Index: 6d
    startx_length f, 2
      0x00000000004011d8 <calc+0x18>..
      0x00000000004011d9 <calc+0x19>
        [ 0] reg1
    startx_length 10, 9
      0x00000000004011da <calc+0x1a>..
      0x00000000004011e2 <calc+0x22>
        [ 0] reg5
    end_of_list

EOF

# GNU DebugFission split-dwarf variant. Still uses .debug_loc, but now in
# .dwo file, with somewhat similar, but different encoding from DWARF5.
testfiles testfile-splitdwarf-4 testfile-hello4.dwo testfile-world4.dwo
testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=loc --dwarf-skeleton=testfile-splitdwarf-4 testfile-hello4.dwo testfile-world4.dwo <<\EOF

testfile-hello4.dwo:


DWARF section [ 3] '.debug_loc.dwo' at offset 0x253:

 CU [     b] base: 0x0000000000401160 <foo>
 [     0] range 401160, 40116a
          0x0000000000401160 <foo>..
          0x0000000000401169 <foo+0x9>
           [ 0] reg5
          range 40116a, 401194
          0x000000000040116a <foo+0xa>..
          0x0000000000401193 <foo+0x33>
           [ 0] GNU_entry_value:
                [ 0] reg5
           [ 3] stack_value
 [    16] range 40117b, 40118d
          0x000000000040117b <foo+0x1b>..
          0x000000000040118c <foo+0x2c>
           [ 0] GNU_addr_index [18] 0x404038 <m>
 [    21] range 40117b, 401181
          0x000000000040117b <foo+0x1b>..
          0x0000000000401180 <foo+0x20>
           [ 0] reg5
 [    2b] range 40117b, 401187
          0x000000000040117b <foo+0x1b>..
          0x0000000000401186 <foo+0x26>
           [ 0] reg5
          range 401189, 40118d
          0x0000000000401189 <foo+0x29>..
          0x000000000040118c <foo+0x2c>
           [ 0] reg5
 [    3e] range 401181, 401187
          0x0000000000401181 <foo+0x21>..
          0x0000000000401186 <foo+0x26>
           [ 0] reg5
          range 401189, 40118d
          0x0000000000401189 <foo+0x29>..
          0x000000000040118c <foo+0x2c>
           [ 0] reg5
 [    51] range 401181, 40118d
          0x0000000000401181 <foo+0x21>..
          0x000000000040118c <foo+0x2c>
           [ 0] reg5
 [    5b] range 40118d, 401193
          0x000000000040118d <foo+0x2d>..
          0x0000000000401192 <foo+0x32>
           [ 0] reg5
 [    65] range 4011a0, 4011af
          0x00000000004011a0 <baz>..
          0x00000000004011ae <baz+0xe>
           [ 0] reg5
          range 4011af, 4011b1
          0x00000000004011af <baz+0xf>..
          0x00000000004011b0 <baz+0x10>
           [ 0] GNU_entry_value:
                [ 0] reg5
           [ 3] stack_value
 [    7b] range 4011a0, 4011b0
          0x00000000004011a0 <baz>..
          0x00000000004011af <baz+0xf>
           [ 0] reg5

testfile-world4.dwo:


DWARF section [ 3] '.debug_loc.dwo' at offset 0x225:

 CU [     b] base: 000000000000000000
 [     0] range 401060, 401074
          0x0000000000401060 <main>..
          0x0000000000401073 <main+0x13>
           [ 0] reg5
          range 401074, 401080
          0x0000000000401074 <main+0x14>..
          0x000000000040107f <main+0x1f>
           [ 0] GNU_entry_value:
                [ 0] reg5
           [ 3] stack_value
 [    16] range 401060, 401078
          0x0000000000401060 <main>..
          0x0000000000401077 <main+0x17>
           [ 0] reg4
          range 401078, 40107e
          0x0000000000401078 <main+0x18>..
          0x000000000040107d <main+0x1d>
           [ 0] GNU_entry_value:
                [ 0] reg4
           [ 3] stack_value
 [    2c] range 401071, 401078
          0x0000000000401071 <main+0x11>..
          0x0000000000401077 <main+0x17>
           [ 0] reg0
 [    36] range 4011c0, 4011c8
          0x00000000004011c0 <calc>..
          0x00000000004011c7 <calc+0x7>
           [ 0] reg5
          range 4011c8, 4011eb
          0x00000000004011c8 <calc+0x8>..
          0x00000000004011ea <calc+0x2a>
           [ 0] GNU_entry_value:
                [ 0] reg5
           [ 3] stack_value
 [    4c] range 4011d8, 4011e3
          0x00000000004011d8 <calc+0x18>..
          0x00000000004011e2 <calc+0x22>
           [ 0] reg0
 [    56] range 4011d8, 4011da
          0x00000000004011d8 <calc+0x18>..
          0x00000000004011d9 <calc+0x19>
           [ 0] reg1
          range 4011da, 4011df
          0x00000000004011da <calc+0x1a>..
          0x00000000004011de <calc+0x1e>
           [ 0] reg5
          range 4011df, 4011e3
          0x00000000004011df <calc+0x1f>..
          0x00000000004011e2 <calc+0x22>
           [ 0] GNU_entry_value:
                [ 0] reg5
           [ 3] deref_size 1
           [ 5] const1u 56
           [ 7] shl
           [ 8] const1u 56
           [10] shra
           [11] stack_value
 [    7d] range 4011d8, 4011da
          0x00000000004011d8 <calc+0x18>..
          0x00000000004011d9 <calc+0x19>
           [ 0] reg1
          range 4011da, 4011e3
          0x00000000004011da <calc+0x1a>..
          0x00000000004011e2 <calc+0x22>
           [ 0] reg5
EOF

# Partial dwarf-4 and partial GNU DebugFission split-dwarf.
#
# = popcount.c =
#
# int popcount (unsigned char u)
# {
#   int c = 0;
#   while (u != 0)
#     {
#       if ((u & 1) == 1)
#         c++;
#       u >>= 1;
#     }
#   return c;
# }
#
# = splitdwarf4-not-split4.c =
#
# extern int popcount (unsigned char);
#
# int main (int argc, char **argv)
# {
#   int i;
#   int p = argc;
#   for (i = 0; i < argc;  ++i)
#     p += popcount (argv[i][0]);
#   i += p;
#   return i;
# }
#
# gcc -gdwarf-4 -O2 -c popcount.c
# gcc -gdwarf-4 -gsplit-dwarf -O2 -c splitdwarf4-not-split4.c
# gcc -o testfile-splitdwarf4-not-split4 splitdwarf4-not-split4.o popcount.o
# eu-strip -f testfile-splitdwarf4-not-split4.debug \
#   testfile-splitdwarf4-not-split4

testfiles testfile-splitdwarf4-not-split4.debug
testfiles splitdwarf4-not-split4.dwo

testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=info+ --debug-dump=loc testfile-splitdwarf4-not-split4.debug <<\EOF

DWARF section [28] '.debug_info' at offset 0x330:
 [Offset]
 Compilation unit at offset 0:
 Version: 4, Abbreviation section offset: 0, Address size: 8, Offset size: 4
 Unit type: skeleton (4), Unit id: 0x3d909d7bd0e69c0b
 [     b]  compile_unit         abbrev: 1
           ranges               (sec_offset) range list [     0]
           low_pc               (addr) 000000000000000000
           stmt_list            (sec_offset) 0
           GNU_dwo_name         (strp) "splitdwarf4-not-split4.dwo"
           comp_dir             (strp) "/tmp"
           GNU_pubnames         (flag_present) yes
           GNU_addr_base        (sec_offset) address base [     0]
           GNU_dwo_id           (data8) 0x3d909d7bd0e69c0b
           GNU_ranges_base      (sec_offset) 0
 Split compilation unit at offset 0:
 Version: 4, Abbreviation section offset: 0, Address size: 8, Offset size: 4
 Unit type: skeleton (4), Unit id: 0x3d909d7bd0e69c0b
 {     b}  compile_unit         abbrev: 1
           producer             (GNU_str_index) "GNU C17 9.0.0 20180528 (experimental) -mtune=generic -march=x86-64 -gdwarf-4 -gsplit-dwarf -O2"
           language             (data1) C99 (12)
           name                 (GNU_str_index) "splitdwarf4-not-split4.c"
           comp_dir             (GNU_str_index) "/tmp"
           GNU_dwo_id           (data8) 0x3d909d7bd0e69c0b
 {    18}    subprogram           abbrev: 2
             external             (flag_present) yes
             name                 (GNU_str_index) "main"
             decl_file            (data1) splitdwarf4-not-split4.c (1)
             decl_line            (data1) 3
             decl_column          (data1) 5
             prototyped           (flag_present) yes
             type                 (ref4) {    6d}
             low_pc               (GNU_addr_index) [4] 0x0000000000401050 <main>
             high_pc              (data8) 76 (0x000000000040109c <_start>)
             frame_base           (exprloc) 
              [ 0] call_frame_cfa
             GNU_all_call_sites   (flag_present) yes
             sibling              (ref4) {    6d}
 {    30}      formal_parameter     abbrev: 3
               name                 (GNU_str_index) "argc"
               decl_file            (data1) splitdwarf4-not-split4.c (1)
               decl_line            (data1) 3
               decl_column          (data1) 15
               type                 (ref4) {    6d}
               location             (sec_offset) location list [     0]
 {    3d}      formal_parameter     abbrev: 3
               name                 (GNU_str_index) "argv"
               decl_file            (data1) splitdwarf4-not-split4.c (1)
               decl_line            (data1) 3
               decl_column          (data1) 28
               type                 (ref4) {    74}
               location             (sec_offset) location list [    28]
 {    4a}      variable             abbrev: 4
               name                 (string) "i"
               decl_file            (data1) splitdwarf4-not-split4.c (1)
               decl_line            (data1) 5
               decl_column          (data1) 7
               type                 (ref4) {    6d}
               location             (sec_offset) location list [    47]
 {    58}      variable             abbrev: 4
               name                 (string) "p"
               decl_file            (data1) splitdwarf4-not-split4.c (1)
               decl_line            (data1) 6
               decl_column          (data1) 7
               type                 (ref4) {    6d}
               location             (sec_offset) location list [    73]
 {    66}      GNU_call_site        abbrev: 5
               low_pc               (GNU_addr_index) [1] 0x000000000040107f <main+0x2f>
               abstract_origin      (ref4) {    84}
 {    6d}    base_type            abbrev: 6
             byte_size            (data1) 4
             encoding             (data1) signed (5)
             name                 (string) "int"
 {    74}    pointer_type         abbrev: 7
             byte_size            (data1) 8
             type                 (ref4) {    7a}
 {    7a}    pointer_type         abbrev: 7
             byte_size            (data1) 8
             type                 (ref4) {    80}
 {    80}    base_type            abbrev: 8
             byte_size            (data1) 1
             encoding             (data1) signed_char (6)
             name                 (GNU_str_index) "char"
 {    84}    subprogram           abbrev: 9
             external             (flag_present) yes
             declaration          (flag_present) yes
             linkage_name         (GNU_str_index) "popcount"
             name                 (GNU_str_index) "popcount"
             decl_file            (data1) splitdwarf4-not-split4.c (1)
             decl_line            (data1) 1
             decl_column          (data1) 12
 Compilation unit at offset 52:
 Version: 4, Abbreviation section offset: 29, Address size: 8, Offset size: 4
 [    3f]  compile_unit         abbrev: 1
           producer             (strp) "GNU C17 9.0.0 20180528 (experimental) -mtune=generic -march=x86-64 -gdwarf-4 -O2"
           language             (data1) C99 (12)
           name                 (strp) "popcount.c"
           comp_dir             (strp) "/tmp"
           low_pc               (addr) 0x0000000000401180 <popcount>
           high_pc              (data8) 33 (0x00000000004011a1)
           stmt_list            (sec_offset) 145
 [    61]    subprogram           abbrev: 2
             external             (flag_present) yes
             name                 (strp) "popcount"
             decl_file            (data1) popcount.c (1)
             decl_line            (data1) 1
             decl_column          (data1) 5
             prototyped           (flag_present) yes
             type                 (ref4) [    a0]
             low_pc               (addr) 0x0000000000401180 <popcount>
             high_pc              (data8) 33 (0x00000000004011a1)
             frame_base           (exprloc) 
              [ 0] call_frame_cfa
             GNU_all_call_sites   (flag_present) yes
             sibling              (ref4) [    a0]
 [    83]      formal_parameter     abbrev: 3
               name                 (string) "u"
               decl_file            (data1) popcount.c (1)
               decl_line            (data1) 1
               decl_column          (data1) 29
               type                 (ref4) [    a7]
               location             (sec_offset) location list [     0]
 [    91]      variable             abbrev: 4
               name                 (string) "c"
               decl_file            (data1) popcount.c (1)
               decl_line            (data1) 3
               decl_column          (data1) 7
               type                 (ref4) [    a0]
               location             (sec_offset) location list [    60]
 [    a0]    base_type            abbrev: 5
             byte_size            (data1) 4
             encoding             (data1) signed (5)
             name                 (string) "int"
 [    a7]    base_type            abbrev: 6
             byte_size            (data1) 1
             encoding             (data1) unsigned_char (8)
             name                 (strp) "unsigned char"

DWARF section [32] '.debug_loc' at offset 0x5ef:

 CU [    3f] base: 0x0000000000401180 <popcount>
 [     0] range 0, 9
          0x0000000000401180 <popcount>..
          0x0000000000401188 <popcount+0x8>
           [ 0] reg5
          range 9, 1b
          0x0000000000401189 <popcount+0x9>..
          0x000000000040119a <popcount+0x1a>
           [ 0] reg1
          range 1b, 1d
          0x000000000040119b <popcount+0x1b>..
          0x000000000040119c <popcount+0x1c>
           [ 0] breg1 0
           [ 2] lit1
           [ 3] shr
           [ 4] stack_value
          range 1d, 21
          0x000000000040119d <popcount+0x1d>..
          0x00000000004011a0 <popcount+0x20>
           [ 0] reg1
 [    60] range 0, 9
          0x0000000000401180 <popcount>..
          0x0000000000401188 <popcount+0x8>
           [ 0] lit0
           [ 1] stack_value
          range 9, 20
          0x0000000000401189 <popcount+0x9>..
          0x000000000040119f <popcount+0x1f>
           [ 0] reg0
          range 20, 21
          0x00000000004011a0 <popcount+0x20>..
          0x00000000004011a0 <popcount+0x20>
           [ 0] lit0
           [ 1] stack_value
EOF

testrun_compare ${abs_top_builddir}/src/readelf --dwarf-skeleton=testfile-splitdwarf4-not-split4.debug --debug-dump=loc splitdwarf4-not-split4.dwo <<\EOF

DWARF section [ 3] '.debug_loc.dwo' at offset 0x15b:

 CU [     b] base: 000000000000000000
 [     0] range 401050, 40106e
          0x0000000000401050 <main>..
          0x000000000040106d <main+0x1d>
           [ 0] reg5
          range 40106e, 401086
          0x000000000040106e <main+0x1e>..
          0x0000000000401085 <main+0x35>
           [ 0] reg12
          range 401086, 401095
          0x0000000000401086 <main+0x36>..
          0x0000000000401094 <main+0x44>
           [ 0] GNU_entry_value:
                [ 0] reg5
           [ 3] stack_value
          range 401095, 40109c
          0x0000000000401095 <main+0x45>..
          0x000000000040109b <main+0x4b>
           [ 0] reg5
 [    28] range 401050, 40106e
          0x0000000000401050 <main>..
          0x000000000040106d <main+0x1d>
           [ 0] reg4
          range 40106e, 401095
          0x000000000040106e <main+0x1e>..
          0x0000000000401094 <main+0x44>
           [ 0] GNU_entry_value:
                [ 0] reg4
           [ 3] stack_value
          range 401095, 40109c
          0x0000000000401095 <main+0x45>..
          0x000000000040109b <main+0x4b>
           [ 0] reg4
 [    47] range 401050, 40106e
          0x0000000000401050 <main>..
          0x000000000040106d <main+0x1d>
           [ 0] lit0
           [ 1] stack_value
          range 401086, 40108e
          0x0000000000401086 <main+0x36>..
          0x000000000040108d <main+0x3d>
           [ 0] breg12 0
           [ 2] breg6 0
           [ 4] plus
           [ 5] stack_value
          range 40108e, 401095
          0x000000000040108e <main+0x3e>..
          0x0000000000401094 <main+0x44>
           [ 0] reg0
          range 401095, 40109c
          0x0000000000401095 <main+0x45>..
          0x000000000040109b <main+0x4b>
           [ 0] lit0
           [ 1] stack_value
 [    73] range 401050, 40106e
          0x0000000000401050 <main>..
          0x000000000040106d <main+0x1d>
           [ 0] reg5
          range 40106e, 401090
          0x000000000040106e <main+0x1e>..
          0x000000000040108f <main+0x3f>
           [ 0] reg6
          range 401095, 40109c
          0x0000000000401095 <main+0x45>..
          0x000000000040109b <main+0x4b>
           [ 0] reg5
EOF

exit 0
