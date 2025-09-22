#! /bin/sh
# Copyright (C) 2013, 2018 Red Hat, Inc.
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

# Tests readelf --debug-dump=line and --debug-dump=decodedline
# See run-readelf-aranges for testfiles.

testfiles testfilefoobarbaz

testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=line testfilefoobarbaz <<EOF

DWARF section [30] '.debug_line' at offset 0x15f6:

Table at offset 0:

 Length:                         83
 DWARF version:                  2
 Prologue length:                43
 Address size:                   4
 Segment selector size:          0
 Min instruction length:         1
 Max operations per instruction: 1
 Initial value if 'is_stmt':     1
 Line base:                      -5
 Line range:                     14
 Opcode base:                    13

Opcodes:
  [ 1]  0 arguments
  [ 2]  1 argument
  [ 3]  1 argument
  [ 4]  1 argument
  [ 5]  1 argument
  [ 6]  0 arguments
  [ 7]  0 arguments
  [ 8]  0 arguments
  [ 9]  1 argument
  [10]  0 arguments
  [11]  0 arguments
  [12]  1 argument

Directory table:

File name table:
 Entry Dir   Time      Size      Name
 1     0     0         0         foo.c
 2     0     0         0         foobarbaz.h

Line number statements:
 [    35] extended opcode 2:  set address to 0x80482f0 <main>
 [    3c] advance line by constant 15 to 16
 [    3e] copy
 [    3f] special opcode 159: address+10 = 0x80482fa <main+0xa>, line+1 = 17
 [    40] special opcode 117: address+7 = 0x8048301 <main+0x11>, line+1 = 18
 [    41] advance line by constant -9 to 9
 [    43] special opcode 200: address+13 = 0x804830e <main+0x1e>, line+0 = 9
 [    44] special opcode 48: address+2 = 0x8048310 <main+0x20>, line+2 = 11
 [    45] special opcode 58: address+3 = 0x8048313 <main+0x23>, line-2 = 9
 [    46] special opcode 48: address+2 = 0x8048315 <main+0x25>, line+2 = 11
 [    47] special opcode 44: address+2 = 0x8048317 <main+0x27>, line-2 = 9
 [    48] advance line by constant 13 to 22
 [    4a] special opcode 46: address+2 = 0x8048319 <main+0x29>, line+0 = 22
 [    4b] advance line by constant -13 to 9
 [    4d] special opcode 60: address+3 = 0x804831c <main+0x2c>, line+0 = 9
 [    4e] advance line by constant 12 to 21
 [    50] special opcode 60: address+3 = 0x804831f <main+0x2f>, line+0 = 21
 [    51] special opcode 61: address+3 = 0x8048322 <main+0x32>, line+1 = 22
 [    52] advance address by 2 to 0x8048324
 [    54] extended opcode 1:  end of sequence

Table at offset 87:

 Length:                         72
 DWARF version:                  2
 Prologue length:                28
 Address size:                   4
 Segment selector size:          0
 Min instruction length:         1
 Max operations per instruction: 1
 Initial value if 'is_stmt':     1
 Line base:                      -5
 Line range:                     14
 Opcode base:                    13

Opcodes:
  [ 1]  0 arguments
  [ 2]  1 argument
  [ 3]  1 argument
  [ 4]  1 argument
  [ 5]  1 argument
  [ 6]  0 arguments
  [ 7]  0 arguments
  [ 8]  0 arguments
  [ 9]  1 argument
  [10]  0 arguments
  [11]  0 arguments
  [12]  1 argument

Directory table:

File name table:
 Entry Dir   Time      Size      Name
 1     0     0         0         bar.c

Line number statements:
 [    7d] extended opcode 2:  set address to 0x8048330 <nobar>
 [    84] advance line by constant 12 to 13
 [    86] copy
 [    87] special opcode 19: address+0 = 0x8048330 <nobar>, line+1 = 14
 [    88] advance address by 11 to 0x804833b
 [    8a] extended opcode 1:  end of sequence
 [    8d] extended opcode 2:  set address to 0x8048440 <bar>
 [    94] advance line by constant 18 to 19
 [    96] copy
 [    97] special opcode 19: address+0 = 0x8048440 <bar>, line+1 = 20
 [    98] advance line by constant -12 to 8
 [    9a] special opcode 200: address+13 = 0x804844d <bar+0xd>, line+0 = 8
 [    9b] advance line by constant 14 to 22
 [    9d] special opcode 74: address+4 = 0x8048451 <bar+0x11>, line+0 = 22
 [    9e] advance address by 1 to 0x8048452
 [    a0] extended opcode 1:  end of sequence

Table at offset 163:

 Length:                         106
 DWARF version:                  2
 Prologue length:                43
 Address size:                   4
 Segment selector size:          0
 Min instruction length:         1
 Max operations per instruction: 1
 Initial value if 'is_stmt':     1
 Line base:                      -5
 Line range:                     14
 Opcode base:                    13

Opcodes:
  [ 1]  0 arguments
  [ 2]  1 argument
  [ 3]  1 argument
  [ 4]  1 argument
  [ 5]  1 argument
  [ 6]  0 arguments
  [ 7]  0 arguments
  [ 8]  0 arguments
  [ 9]  1 argument
  [10]  0 arguments
  [11]  0 arguments
  [12]  1 argument

Directory table:

File name table:
 Entry Dir   Time      Size      Name
 1     0     0         0         baz.c
 2     0     0         0         foobarbaz.h

Line number statements:
 [    d8] extended opcode 2:  set address to 0x8048340 <nobaz>
 [    df] advance line by constant 12 to 13
 [    e1] copy
 [    e2] special opcode 19: address+0 = 0x8048340 <nobaz>, line+1 = 14
 [    e3] advance address by 11 to 0x804834b
 [    e5] extended opcode 1:  end of sequence
 [    e8] extended opcode 2:  set address to 0x8048460 <baz>
 [    ef] advance line by constant 18 to 19
 [    f1] copy
 [    f2] special opcode 74: address+4 = 0x8048464 <baz+0x4>, line+0 = 19
 [    f3] special opcode 75: address+4 = 0x8048468 <baz+0x8>, line+1 = 20
 [    f4] extended opcode 4:  set discriminator to 1
 [    f8] special opcode 78: address+4 = 0x804846c <baz+0xc>, line+4 = 24
 [    f9] special opcode 187: address+12 = 0x8048478 <baz+0x18>, line+1 = 25
 [    fa] special opcode 87: address+5 = 0x804847d <baz+0x1d>, line-1 = 24
 [    fb] special opcode 61: address+3 = 0x8048480 <baz+0x20>, line+1 = 25
 [    fc] special opcode 101: address+6 = 0x8048486 <baz+0x26>, line-1 = 24
 [    fd] special opcode 61: address+3 = 0x8048489 <baz+0x29>, line+1 = 25
 [    fe] special opcode 87: address+5 = 0x804848e <baz+0x2e>, line-1 = 24
 [    ff] advance line by constant -16 to 8
 [   101] special opcode 46: address+2 = 0x8048490 <baz+0x30>, line+0 = 8
 [   102] advance line by constant 20 to 28
 [   104] special opcode 186: address+12 = 0x804849c <baz+0x3c>, line+0 = 28
 [   105] advance line by constant -20 to 8
 [   107] special opcode 88: address+5 = 0x80484a1 <baz+0x41>, line+0 = 8
 [   108] advance line by constant 13 to 21
 [   10a] advance address by constant 17 to 0x80484b2 <baz+0x52>
 [   10b] special opcode 32: address+1 = 0x80484b3 <baz+0x53>, line+0 = 21
 [   10c] advance address by 9 to 0x80484bc
 [   10e] extended opcode 1:  end of sequence
EOF

testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=decodedline testfilefoobarbaz <<\EOF

DWARF section [30] '.debug_line' at offset 0x15f6:

 CU [b] foo.c
  line:col SBPE* disc isa op address (Statement Block Prologue Epilogue *End)
  /home/mark/src/tests/foobarbaz/foo.c (mtime: 0, length: 0)
    16:0   S        0   0  0 0x080482f0 <main>
    17:0   S        0   0  0 0x080482fa <main+0xa>
    18:0   S        0   0  0 0x08048301 <main+0x11>
     9:0   S        0   0  0 0x0804830e <main+0x1e>
    11:0   S        0   0  0 0x08048310 <main+0x20>
     9:0   S        0   0  0 0x08048313 <main+0x23>
    11:0   S        0   0  0 0x08048315 <main+0x25>
     9:0   S        0   0  0 0x08048317 <main+0x27>
    22:0   S        0   0  0 0x08048319 <main+0x29>
     9:0   S        0   0  0 0x0804831c <main+0x2c>
    21:0   S        0   0  0 0x0804831f <main+0x2f>
    22:0   S        0   0  0 0x08048322 <main+0x32>
    22:0   S   *    0   0  0 0x08048323 <main+0x33>

 CU [141] bar.c
  line:col SBPE* disc isa op address (Statement Block Prologue Epilogue *End)
  /home/mark/src/tests/foobarbaz/bar.c (mtime: 0, length: 0)
    13:0   S        0   0  0 0x08048330 <nobar>
    14:0   S        0   0  0 0x08048330 <nobar>
    14:0   S   *    0   0  0 0x0804833a <nobar+0xa>

    19:0   S        0   0  0 0x08048440 <bar>
    20:0   S        0   0  0 0x08048440 <bar>
     8:0   S        0   0  0 0x0804844d <bar+0xd>
    22:0   S        0   0  0 0x08048451 <bar+0x11>
    22:0   S   *    0   0  0 0x08048451 <bar+0x11>

 CU [1dc] baz.c
  line:col SBPE* disc isa op address (Statement Block Prologue Epilogue *End)
  /home/mark/src/tests/foobarbaz/baz.c (mtime: 0, length: 0)
    13:0   S        0   0  0 0x08048340 <nobaz>
    14:0   S        0   0  0 0x08048340 <nobaz>
    14:0   S   *    0   0  0 0x0804834a <nobaz+0xa>

    19:0   S        0   0  0 0x08048460 <baz>
    19:0   S        0   0  0 0x08048464 <baz+0x4>
    20:0   S        0   0  0 0x08048468 <baz+0x8>
    24:0   S        1   0  0 0x0804846c <baz+0xc>
    25:0   S        0   0  0 0x08048478 <baz+0x18>
    24:0   S        0   0  0 0x0804847d <baz+0x1d>
    25:0   S        0   0  0 0x08048480 <baz+0x20>
    24:0   S        0   0  0 0x08048486 <baz+0x26>
    25:0   S        0   0  0 0x08048489 <baz+0x29>
    24:0   S        0   0  0 0x0804848e <baz+0x2e>
     8:0   S        0   0  0 0x08048490 <baz+0x30>
    28:0   S        0   0  0 0x0804849c <baz+0x3c>
     8:0   S        0   0  0 0x080484a1 <baz+0x41>
    21:0   S        0   0  0 0x080484b3 <baz+0x53>
    21:0   S   *    0   0  0 0x080484bb <baz+0x5b>

EOF

# A .debug_line table with minimum instruction length > 1.
#
# = hello.c
# #include <stdio.h>
#
# int
# main (int argc, char **argv)
# {
#   printf ("Hello, %s\n", (argc > 0
# 			  ? argv[1]: "World"));
#   return 0;
# }
#
# clang version 5.0.1 (tags/RELEASE_501/final)
# Target: powerpc64-unknown-linux-gnu
# clang -g -O2 -o testfile-ppc64-min-instr hello.c
testfiles testfile-ppc64-min-instr

testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=line testfile-ppc64-min-instr <<\EOF

DWARF section [29] '.debug_line' at offset 0xdf6:

Table at offset 0:

 Length:                         69
 DWARF version:                  2
 Prologue length:                30
 Address size:                   8
 Segment selector size:          0
 Min instruction length:         4
 Max operations per instruction: 1
 Initial value if 'is_stmt':     1
 Line base:                      -5
 Line range:                     14
 Opcode base:                    13

Opcodes:
  [ 1]  0 arguments
  [ 2]  1 argument
  [ 3]  1 argument
  [ 4]  1 argument
  [ 5]  1 argument
  [ 6]  0 arguments
  [ 7]  0 arguments
  [ 8]  0 arguments
  [ 9]  1 argument
  [10]  0 arguments
  [11]  0 arguments
  [12]  1 argument

Directory table:

File name table:
 Entry Dir   Time      Size      Name
 1     0     0         0         hello.c

Line number statements:
 [    28] extended opcode 2:  set address to 0x100005a4 <main>
 [    33] special opcode 22: address+0 = 0x100005a4 <main>, line+4 = 5
 [    34] set column to 27
 [    36] set prologue end flag
 [    37] special opcode 19: address+0 = 0x100005a4 <main>, line+1 = 6
 [    38] set column to 8
 [    3a] special opcode 47: address+8 = 0x100005ac <main+0x8>, line+1 = 7
 [    3b] set 'is_stmt' to 0
 [    3c] advance line by constant -7 to 0
 [    3e] special opcode 32: address+4 = 0x100005b0 <main+0xc>, line+0 = 0
 [    3f] set column to 3
 [    41] set 'is_stmt' to 1
 [    42] special opcode 108: address+24 = 0x100005c8 <main+0x24>, line+6 = 6
 [    43] special opcode 76: address+16 = 0x100005d8 <main+0x34>, line+2 = 8
 [    44] advance address by 32 to 0x100005f8
 [    46] extended opcode 1:  end of sequence
EOF

testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=decodedline testfile-ppc64-min-instr <<\EOF

DWARF section [29] '.debug_line' at offset 0xdf6:

 CU [b] hello.c
  line:col SBPE* disc isa op address (Statement Block Prologue Epilogue *End)
  /home/fedora/mjw/hello.c (mtime: 0, length: 0)
     5:0   S        0   0  0 0x00000000100005a4 <main>
     6:27  S P      0   0  0 0x00000000100005a4 <main>
     7:8   S        0   0  0 0x00000000100005ac <main+0x8>
     0:8            0   0  0 0x00000000100005b0 <main+0xc>
     6:3   S        0   0  0 0x00000000100005c8 <main+0x24>
     8:3   S        0   0  0 0x00000000100005d8 <main+0x34>
     8:3   S   *    0   0  0 0x00000000100005f7 <main+0x53>

EOF

# Two tests for the same code but encoded using DWARF4 or DWARF5.
# Output is identical except for the section offset and CU numbers.
# See tests/testfile-dwarf-45.source.

testfiles testfile-dwarf-4 testfile-dwarf-5

testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=decodedline testfile-dwarf-4 << \EOF

DWARF section [29] '.debug_line' at offset 0x1734:

 CU [b] hello.c
  line:col SBPE* disc isa op address (Statement Block Prologue Epilogue *End)
  /var/tmp/hello/hello.c (mtime: 0, length: 0)
    21:0   S        0   0  0 0x0000000000400510 <foo>
    22:1   S        0   0  0 0x0000000000400510 <foo>
    22:3            0   0  0 0x0000000000400510 <foo>
    25:6            0   0  0 0x0000000000400514 <foo+0x4>
    25:34  S        0   0  0 0x000000000040051a <foo+0xa>
    25:3            0   0  0 0x000000000040051a <foo+0xa>
    26:34           0   0  0 0x000000000040051e <foo+0xe>
    25:1            1   0  0 0x0000000000400528 <foo+0x18>
  /var/tmp/hello/hello.h (mtime: 0, length: 0)
     7:18  S        0   0  0 0x000000000040052b <foo+0x1b>
     9:3   S        0   0  0 0x000000000040052b <foo+0x1b>
     9:3            0   0  0 0x000000000040052b <foo+0x1b>
    10:6   S        0   0  0 0x000000000040052f <foo+0x1f>
    10:5            0   0  0 0x000000000040052f <foo+0x1f>
    12:7   S        0   0  0 0x0000000000400531 <foo+0x21>
  /var/tmp/hello/hello.c (mtime: 0, length: 0)
    10:3   S        0   0  0 0x0000000000400531 <foo+0x21>
    12:3   S        0   0  0 0x0000000000400531 <foo+0x21>
    12:3            0   0  0 0x0000000000400531 <foo+0x21>
    13:6   S        0   0  0 0x0000000000400535 <foo+0x25>
    13:5            0   0  0 0x0000000000400535 <foo+0x25>
    15:7   S        0   0  0 0x0000000000400539 <foo+0x29>
    22:3   S        0   0  0 0x0000000000400539 <foo+0x29>
    22:3            0   0  0 0x0000000000400539 <foo+0x29>
    23:6   S        0   0  0 0x000000000040053d <foo+0x2d>
    23:5            0   0  0 0x000000000040053d <foo+0x2d>
     9:12  S        0   0  0 0x0000000000400550 <baz>
    10:1   S        0   0  0 0x0000000000400550 <baz>
    12:3   S        0   0  0 0x0000000000400550 <baz>
    12:3            0   0  0 0x0000000000400550 <baz>
    13:9            0   0  0 0x0000000000400556 <baz+0x6>
    15:7   S        0   0  0 0x000000000040055f <baz+0xf>
    15:3            0   0  0 0x000000000040055f <baz+0xf>
    15:7       *    0   0  0 0x0000000000400560 <baz+0x10>

 CU [21c] world.c
  line:col SBPE* disc isa op address (Statement Block Prologue Epilogue *End)
  /var/tmp/hello/world.c (mtime: 0, length: 0)
    15:0   S        0   0  0 0x0000000000400410 <main>
    16:1   S        0   0  0 0x0000000000400410 <main>
    17:3   S        0   0  0 0x0000000000400410 <main>
    15:3            0   0  0 0x0000000000400410 <main>
    17:1            0   0  0 0x0000000000400419 <main+0x9>
    18:6   S        0   0  0 0x000000000040041e <main+0xe>
    18:5            0   0  0 0x000000000040041e <main+0xe>
    22:7   S        0   0  0 0x0000000000400421 <main+0x11>
    22:3   S   *    0   0  0 0x000000000040042f <main+0x1f>

     6:0   S        0   0  0 0x0000000000400570 <calc>
     7:1   S        0   0  0 0x0000000000400570 <calc>
     7:3            0   0  0 0x0000000000400570 <calc>
     7:6            1   0  0 0x0000000000400575 <calc+0x5>
     7:24           0   0  0 0x0000000000400578 <calc+0x8>
    10:17  S        0   0  0 0x000000000040057d <calc+0xd>
    10:3            0   0  0 0x000000000040057d <calc+0xd>
  /var/tmp/hello/hello.h (mtime: 0, length: 0)
    10:10           0   0  0 0x0000000000400583 <calc+0x13>
  /var/tmp/hello/world.c (mtime: 0, length: 0)
    10:7            0   0  0 0x0000000000400585 <calc+0x15>
  /var/tmp/hello/hello.h (mtime: 0, length: 0)
     7:10  S        0   0  0 0x0000000000400588 <calc+0x18>
     9:3   S        0   0  0 0x0000000000400588 <calc+0x18>
    10:3            0   0  0 0x0000000000400588 <calc+0x18>
    12:7   S        0   0  0 0x000000000040058f <calc+0x1f>
    12:3            0   0  0 0x000000000040058f <calc+0x1f>
  /var/tmp/hello/world.c (mtime: 0, length: 0)
    11:10           0   0  0 0x0000000000400598 <calc+0x28>
    11:1       *    0   0  0 0x000000000040059a <calc+0x2a>

EOF

testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=decodedline testfile-dwarf-5 << \EOF

DWARF section [29] '.debug_line' at offset 0x171f:

 CU [c] hello.c
  line:col SBPE* disc isa op address (Statement Block Prologue Epilogue *End)
  /var/tmp/hello/hello.c (mtime: 0, length: 0)
    21:0   S        0   0  0 0x0000000000400510 <foo>
    22:1   S        0   0  0 0x0000000000400510 <foo>
    22:3            0   0  0 0x0000000000400510 <foo>
    25:6            0   0  0 0x0000000000400514 <foo+0x4>
    25:34  S        0   0  0 0x000000000040051a <foo+0xa>
    25:3            0   0  0 0x000000000040051a <foo+0xa>
    26:34           0   0  0 0x000000000040051e <foo+0xe>
    25:1            1   0  0 0x0000000000400528 <foo+0x18>
  /var/tmp/hello/hello.h (mtime: 0, length: 0)
     7:18  S        0   0  0 0x000000000040052b <foo+0x1b>
     9:3   S        0   0  0 0x000000000040052b <foo+0x1b>
     9:3            0   0  0 0x000000000040052b <foo+0x1b>
    10:6   S        0   0  0 0x000000000040052f <foo+0x1f>
    10:5            0   0  0 0x000000000040052f <foo+0x1f>
    12:7   S        0   0  0 0x0000000000400531 <foo+0x21>
  /var/tmp/hello/hello.c (mtime: 0, length: 0)
    10:3   S        0   0  0 0x0000000000400531 <foo+0x21>
    12:3   S        0   0  0 0x0000000000400531 <foo+0x21>
    12:3            0   0  0 0x0000000000400531 <foo+0x21>
    13:6   S        0   0  0 0x0000000000400535 <foo+0x25>
    13:5            0   0  0 0x0000000000400535 <foo+0x25>
    15:7   S        0   0  0 0x0000000000400539 <foo+0x29>
    22:3   S        0   0  0 0x0000000000400539 <foo+0x29>
    22:3            0   0  0 0x0000000000400539 <foo+0x29>
    23:6   S        0   0  0 0x000000000040053d <foo+0x2d>
    23:5            0   0  0 0x000000000040053d <foo+0x2d>
     9:12  S        0   0  0 0x0000000000400550 <baz>
    10:1   S        0   0  0 0x0000000000400550 <baz>
    12:3   S        0   0  0 0x0000000000400550 <baz>
    12:3            0   0  0 0x0000000000400550 <baz>
    13:9            0   0  0 0x0000000000400556 <baz+0x6>
    15:7   S        0   0  0 0x000000000040055f <baz+0xf>
    15:3            0   0  0 0x000000000040055f <baz+0xf>
    15:7       *    0   0  0 0x0000000000400560 <baz+0x10>

 CU [218] world.c
  line:col SBPE* disc isa op address (Statement Block Prologue Epilogue *End)
  /var/tmp/hello/world.c (mtime: 0, length: 0)
    15:0   S        0   0  0 0x0000000000400410 <main>
    16:1   S        0   0  0 0x0000000000400410 <main>
    17:3   S        0   0  0 0x0000000000400410 <main>
    15:3            0   0  0 0x0000000000400410 <main>
    17:1            0   0  0 0x0000000000400419 <main+0x9>
    18:6   S        0   0  0 0x000000000040041e <main+0xe>
    18:5            0   0  0 0x000000000040041e <main+0xe>
    22:7   S        0   0  0 0x0000000000400421 <main+0x11>
    22:3   S   *    0   0  0 0x000000000040042f <main+0x1f>

     6:0   S        0   0  0 0x0000000000400570 <calc>
     7:1   S        0   0  0 0x0000000000400570 <calc>
     7:3            0   0  0 0x0000000000400570 <calc>
     7:6            1   0  0 0x0000000000400575 <calc+0x5>
     7:24           0   0  0 0x0000000000400578 <calc+0x8>
    10:17  S        0   0  0 0x000000000040057d <calc+0xd>
    10:3            0   0  0 0x000000000040057d <calc+0xd>
  /var/tmp/hello/hello.h (mtime: 0, length: 0)
    10:10           0   0  0 0x0000000000400583 <calc+0x13>
  /var/tmp/hello/world.c (mtime: 0, length: 0)
    10:7            0   0  0 0x0000000000400585 <calc+0x15>
  /var/tmp/hello/hello.h (mtime: 0, length: 0)
     7:10  S        0   0  0 0x0000000000400588 <calc+0x18>
     9:3   S        0   0  0 0x0000000000400588 <calc+0x18>
    10:3            0   0  0 0x0000000000400588 <calc+0x18>
    12:7   S        0   0  0 0x000000000040058f <calc+0x1f>
    12:3            0   0  0 0x000000000040058f <calc+0x1f>
  /var/tmp/hello/world.c (mtime: 0, length: 0)
    11:10           0   0  0 0x0000000000400598 <calc+0x28>
    11:1       *    0   0  0 0x000000000040059a <calc+0x2a>

EOF

# After discarding the different offsets in the line number statements,
# the remaining difference between 4 and 5 is (besides the header/length)
# Just the representation of the directory and line tables:

#  Directory table:
# - /opt/local/install/gcc/lib/gcc/x86_64-pc-linux-gnu/9.0.0/include
# +      [path(line_strp)]
# + 0     /var/tmp/hello (90)
# + 1     /opt/local/install/gcc/lib/gcc/x86_64-pc-linux-gnu/9.0.0/include (17)
#
#  File name table:
# - Entry Dir   Time      Size      Name
# - 1     0     0         0         hello.c
# - 2     0     0         0         hello.h
# - 3     1     0         0         stddef.h
# +      [path(line_strp), directory_index(data1)]
# + 0     hello.c (9),  0
# + 1     hello.c (9),  0
# + 2     hello.h (82),  0
# + 3     stddef.h (0),  1
#
#  Directory table:
# - /usr/include
# +      [path(line_strp)]
# + 0     /var/tmp/hello (90)
# + 1     /usr/include (122)
#
#  File name table:
# - Entry Dir   Time      Size      Name
# - 1     0     0         0         world.c
# - 2     0     0         0         hello.h
# - 3     1     0         0         stdlib.h
# +      [path(line_strp), directory_index(data1)]
# + 0     world.c (114),  0
# + 1     world.c (114),  0
# + 2     hello.h (82),  0
# + 3     stdlib.h (105),  1

testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=line testfile-dwarf-4 << \EOF

DWARF section [29] '.debug_line' at offset 0x1734:

Table at offset 0:

 Length:                         608
 DWARF version:                  4
 Prologue length:                119
 Address size:                   8
 Segment selector size:          0
 Min instruction length:         1
 Max operations per instruction: 1
 Initial value if 'is_stmt':     1
 Line base:                      -10
 Line range:                     242
 Opcode base:                    13

Opcodes:
  [ 1]  0 arguments
  [ 2]  1 argument
  [ 3]  1 argument
  [ 4]  1 argument
  [ 5]  1 argument
  [ 6]  0 arguments
  [ 7]  0 arguments
  [ 8]  0 arguments
  [ 9]  1 argument
  [10]  0 arguments
  [11]  0 arguments
  [12]  1 argument

Directory table:
 /opt/local/install/gcc/lib/gcc/x86_64-pc-linux-gnu/9.0.0/include

File name table:
 Entry Dir   Time      Size      Name
 1     0     0         0         hello.c
 2     0     0         0         hello.h
 3     1     0         0         stddef.h

Line number statements:
 [    81] extended opcode 2:  set address to 0x400510 <foo>
 [    8c] special opcode 43: address+0 = 0x400510 <foo>, line+20 = 21
 [    8d] set column to 1
 [    8f] extended opcode 2:  set address to 0x400510 <foo>
 [    9a] special opcode 24: address+0 = 0x400510 <foo>, line+1 = 22
 [    9b] set column to 3
 [    9d] extended opcode 2:  set address to 0x400510 <foo>
 [    a8] set 'is_stmt' to 0
 [    a9] copy
 [    aa] set column to 6
 [    ac] extended opcode 2:  set address to 0x400514 <foo+0x4>
 [    b7] special opcode 26: address+0 = 0x400514 <foo+0x4>, line+3 = 25
 [    b8] set column to 34
 [    ba] extended opcode 2:  set address to 0x40051a <foo+0xa>
 [    c5] set 'is_stmt' to 1
 [    c6] copy
 [    c7] set column to 3
 [    c9] extended opcode 2:  set address to 0x40051a <foo+0xa>
 [    d4] set 'is_stmt' to 0
 [    d5] copy
 [    d6] set column to 34
 [    d8] extended opcode 2:  set address to 0x40051e <foo+0xe>
 [    e3] special opcode 24: address+0 = 0x40051e <foo+0xe>, line+1 = 26
 [    e4] set column to 1
 [    e6] extended opcode 2:  set address to 0x400528 <foo+0x18>
 [    f1] extended opcode 4:  set discriminator to 1
 [    f5] special opcode 22: address+0 = 0x400528 <foo+0x18>, line-1 = 25
 [    f6] set column to 18
 [    f8] extended opcode 2:  set address to 0x40052b <foo+0x1b>
 [   103] set file to 2
 [   105] set 'is_stmt' to 1
 [   106] advance line by constant -18 to 7
 [   108] copy
 [   109] set column to 3
 [   10b] extended opcode 2:  set address to 0x40052b <foo+0x1b>
 [   116] special opcode 25: address+0 = 0x40052b <foo+0x1b>, line+2 = 9
 [   117] set column to 3
 [   119] extended opcode 2:  set address to 0x40052b <foo+0x1b>
 [   124] set 'is_stmt' to 0
 [   125] copy
 [   126] set column to 6
 [   128] extended opcode 2:  set address to 0x40052f <foo+0x1f>
 [   133] extended opcode 4:  set discriminator to 0
 [   137] set 'is_stmt' to 1
 [   138] special opcode 24: address+0 = 0x40052f <foo+0x1f>, line+1 = 10
 [   139] set column to 5
 [   13b] extended opcode 2:  set address to 0x40052f <foo+0x1f>
 [   146] set 'is_stmt' to 0
 [   147] copy
 [   148] set column to 7
 [   14a] extended opcode 2:  set address to 0x400531 <foo+0x21>
 [   155] set 'is_stmt' to 1
 [   156] special opcode 25: address+0 = 0x400531 <foo+0x21>, line+2 = 12
 [   157] set column to 3
 [   159] extended opcode 2:  set address to 0x400531 <foo+0x21>
 [   164] set file to 1
 [   166] special opcode 21: address+0 = 0x400531 <foo+0x21>, line-2 = 10
 [   167] set column to 3
 [   169] extended opcode 2:  set address to 0x400531 <foo+0x21>
 [   174] special opcode 25: address+0 = 0x400531 <foo+0x21>, line+2 = 12
 [   175] set column to 3
 [   177] extended opcode 2:  set address to 0x400531 <foo+0x21>
 [   182] set 'is_stmt' to 0
 [   183] copy
 [   184] set column to 6
 [   186] extended opcode 2:  set address to 0x400535 <foo+0x25>
 [   191] set 'is_stmt' to 1
 [   192] special opcode 24: address+0 = 0x400535 <foo+0x25>, line+1 = 13
 [   193] set column to 5
 [   195] extended opcode 2:  set address to 0x400535 <foo+0x25>
 [   1a0] set 'is_stmt' to 0
 [   1a1] copy
 [   1a2] set column to 7
 [   1a4] extended opcode 2:  set address to 0x400539 <foo+0x29>
 [   1af] set 'is_stmt' to 1
 [   1b0] special opcode 25: address+0 = 0x400539 <foo+0x29>, line+2 = 15
 [   1b1] set column to 3
 [   1b3] extended opcode 2:  set address to 0x400539 <foo+0x29>
 [   1be] special opcode 30: address+0 = 0x400539 <foo+0x29>, line+7 = 22
 [   1bf] set column to 3
 [   1c1] extended opcode 2:  set address to 0x400539 <foo+0x29>
 [   1cc] set 'is_stmt' to 0
 [   1cd] copy
 [   1ce] set column to 6
 [   1d0] extended opcode 2:  set address to 0x40053d <foo+0x2d>
 [   1db] set 'is_stmt' to 1
 [   1dc] special opcode 24: address+0 = 0x40053d <foo+0x2d>, line+1 = 23
 [   1dd] set column to 5
 [   1df] extended opcode 2:  set address to 0x40053d <foo+0x2d>
 [   1ea] set 'is_stmt' to 0
 [   1eb] copy
 [   1ec] set column to 12
 [   1ee] extended opcode 2:  set address to 0x400550 <baz>
 [   1f9] set 'is_stmt' to 1
 [   1fa] advance line by constant -14 to 9
 [   1fc] copy
 [   1fd] set column to 1
 [   1ff] extended opcode 2:  set address to 0x400550 <baz>
 [   20a] special opcode 24: address+0 = 0x400550 <baz>, line+1 = 10
 [   20b] set column to 3
 [   20d] extended opcode 2:  set address to 0x400550 <baz>
 [   218] special opcode 25: address+0 = 0x400550 <baz>, line+2 = 12
 [   219] set column to 3
 [   21b] extended opcode 2:  set address to 0x400550 <baz>
 [   226] set 'is_stmt' to 0
 [   227] copy
 [   228] set column to 9
 [   22a] extended opcode 2:  set address to 0x400556 <baz+0x6>
 [   235] special opcode 24: address+0 = 0x400556 <baz+0x6>, line+1 = 13
 [   236] set column to 7
 [   238] extended opcode 2:  set address to 0x40055f <baz+0xf>
 [   243] set 'is_stmt' to 1
 [   244] special opcode 25: address+0 = 0x40055f <baz+0xf>, line+2 = 15
 [   245] set column to 3
 [   247] extended opcode 2:  set address to 0x40055f <baz+0xf>
 [   252] set 'is_stmt' to 0
 [   253] copy
 [   254] set column to 7
 [   256] extended opcode 2:  set address to 0x400561
 [   261] extended opcode 1:  end of sequence

Table at offset 612:

 Length:                         450
 DWARF version:                  4
 Prologue length:                67
 Address size:                   8
 Segment selector size:          0
 Min instruction length:         1
 Max operations per instruction: 1
 Initial value if 'is_stmt':     1
 Line base:                      -10
 Line range:                     242
 Opcode base:                    13

Opcodes:
  [ 1]  0 arguments
  [ 2]  1 argument
  [ 3]  1 argument
  [ 4]  1 argument
  [ 5]  1 argument
  [ 6]  0 arguments
  [ 7]  0 arguments
  [ 8]  0 arguments
  [ 9]  1 argument
  [10]  0 arguments
  [11]  0 arguments
  [12]  1 argument

Directory table:
 /usr/include

File name table:
 Entry Dir   Time      Size      Name
 1     0     0         0         world.c
 2     0     0         0         hello.h
 3     1     0         0         stdlib.h

Line number statements:
 [   2b1] extended opcode 2:  set address to 0x400410 <main>
 [   2bc] special opcode 37: address+0 = 0x400410 <main>, line+14 = 15
 [   2bd] set column to 1
 [   2bf] extended opcode 2:  set address to 0x400410 <main>
 [   2ca] special opcode 24: address+0 = 0x400410 <main>, line+1 = 16
 [   2cb] set column to 3
 [   2cd] extended opcode 2:  set address to 0x400410 <main>
 [   2d8] special opcode 24: address+0 = 0x400410 <main>, line+1 = 17
 [   2d9] set column to 3
 [   2db] extended opcode 2:  set address to 0x400410 <main>
 [   2e6] set 'is_stmt' to 0
 [   2e7] special opcode 21: address+0 = 0x400410 <main>, line-2 = 15
 [   2e8] set column to 1
 [   2ea] extended opcode 2:  set address to 0x400419 <main+0x9>
 [   2f5] special opcode 25: address+0 = 0x400419 <main+0x9>, line+2 = 17
 [   2f6] set column to 6
 [   2f8] extended opcode 2:  set address to 0x40041e <main+0xe>
 [   303] set 'is_stmt' to 1
 [   304] special opcode 24: address+0 = 0x40041e <main+0xe>, line+1 = 18
 [   305] set column to 5
 [   307] extended opcode 2:  set address to 0x40041e <main+0xe>
 [   312] set 'is_stmt' to 0
 [   313] copy
 [   314] set column to 7
 [   316] extended opcode 2:  set address to 0x400421 <main+0x11>
 [   321] set 'is_stmt' to 1
 [   322] special opcode 27: address+0 = 0x400421 <main+0x11>, line+4 = 22
 [   323] set column to 3
 [   325] extended opcode 2:  set address to 0x400430 <_start>
 [   330] extended opcode 1:  end of sequence
 [   333] extended opcode 2:  set address to 0x400570 <calc>
 [   33e] special opcode 28: address+0 = 0x400570 <calc>, line+5 = 6
 [   33f] set column to 1
 [   341] extended opcode 2:  set address to 0x400570 <calc>
 [   34c] special opcode 24: address+0 = 0x400570 <calc>, line+1 = 7
 [   34d] set column to 3
 [   34f] extended opcode 2:  set address to 0x400570 <calc>
 [   35a] set 'is_stmt' to 0
 [   35b] copy
 [   35c] set column to 6
 [   35e] extended opcode 2:  set address to 0x400575 <calc+0x5>
 [   369] extended opcode 4:  set discriminator to 1
 [   36d] copy
 [   36e] set column to 24
 [   370] extended opcode 2:  set address to 0x400578 <calc+0x8>
 [   37b] copy
 [   37c] set column to 17
 [   37e] extended opcode 2:  set address to 0x40057d <calc+0xd>
 [   389] extended opcode 4:  set discriminator to 0
 [   38d] set 'is_stmt' to 1
 [   38e] special opcode 26: address+0 = 0x40057d <calc+0xd>, line+3 = 10
 [   38f] set column to 3
 [   391] extended opcode 2:  set address to 0x40057d <calc+0xd>
 [   39c] set 'is_stmt' to 0
 [   39d] copy
 [   39e] set column to 10
 [   3a0] extended opcode 2:  set address to 0x400583 <calc+0x13>
 [   3ab] set file to 2
 [   3ad] copy
 [   3ae] set column to 7
 [   3b0] extended opcode 2:  set address to 0x400585 <calc+0x15>
 [   3bb] set file to 1
 [   3bd] copy
 [   3be] set column to 10
 [   3c0] extended opcode 2:  set address to 0x400588 <calc+0x18>
 [   3cb] set file to 2
 [   3cd] set 'is_stmt' to 1
 [   3ce] special opcode 20: address+0 = 0x400588 <calc+0x18>, line-3 = 7
 [   3cf] set column to 3
 [   3d1] extended opcode 2:  set address to 0x400588 <calc+0x18>
 [   3dc] special opcode 25: address+0 = 0x400588 <calc+0x18>, line+2 = 9
 [   3dd] set column to 3
 [   3df] extended opcode 2:  set address to 0x400588 <calc+0x18>
 [   3ea] set 'is_stmt' to 0
 [   3eb] special opcode 24: address+0 = 0x400588 <calc+0x18>, line+1 = 10
 [   3ec] set column to 7
 [   3ee] extended opcode 2:  set address to 0x40058f <calc+0x1f>
 [   3f9] set 'is_stmt' to 1
 [   3fa] special opcode 25: address+0 = 0x40058f <calc+0x1f>, line+2 = 12
 [   3fb] set column to 3
 [   3fd] extended opcode 2:  set address to 0x40058f <calc+0x1f>
 [   408] set 'is_stmt' to 0
 [   409] copy
 [   40a] set column to 10
 [   40c] extended opcode 2:  set address to 0x400598 <calc+0x28>
 [   417] set file to 1
 [   419] special opcode 22: address+0 = 0x400598 <calc+0x28>, line-1 = 11
 [   41a] set column to 1
 [   41c] extended opcode 2:  set address to 0x40059b
 [   427] extended opcode 1:  end of sequence
EOF

testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=line testfile-dwarf-5 << \EOF

DWARF section [29] '.debug_line' at offset 0x171f:

Table at offset 0:

 Length:                         547
 DWARF version:                  5
 Prologue length:                56
 Address size:                   8
 Segment selector size:          0
 Min instruction length:         1
 Max operations per instruction: 1
 Initial value if 'is_stmt':     1
 Line base:                      -10
 Line range:                     242
 Opcode base:                    13

Opcodes:
  [ 1]  0 arguments
  [ 2]  1 argument
  [ 3]  1 argument
  [ 4]  1 argument
  [ 5]  1 argument
  [ 6]  0 arguments
  [ 7]  0 arguments
  [ 8]  0 arguments
  [ 9]  1 argument
  [10]  0 arguments
  [11]  0 arguments
  [12]  1 argument

Directory table:
      [path(line_strp)]
 0     /var/tmp/hello (90)
 1     /opt/local/install/gcc/lib/gcc/x86_64-pc-linux-gnu/9.0.0/include (17)

File name table:
      [path(line_strp), directory_index(data1)]
 0     hello.c (9),  0
 1     hello.c (9),  0
 2     hello.h (82),  0
 3     stddef.h (0),  1

Line number statements:
 [    44] extended opcode 2:  set address to 0x400510 <foo>
 [    4f] special opcode 43: address+0 = 0x400510 <foo>, line+20 = 21
 [    50] set column to 1
 [    52] extended opcode 2:  set address to 0x400510 <foo>
 [    5d] special opcode 24: address+0 = 0x400510 <foo>, line+1 = 22
 [    5e] set column to 3
 [    60] extended opcode 2:  set address to 0x400510 <foo>
 [    6b] set 'is_stmt' to 0
 [    6c] copy
 [    6d] set column to 6
 [    6f] extended opcode 2:  set address to 0x400514 <foo+0x4>
 [    7a] special opcode 26: address+0 = 0x400514 <foo+0x4>, line+3 = 25
 [    7b] set column to 34
 [    7d] extended opcode 2:  set address to 0x40051a <foo+0xa>
 [    88] set 'is_stmt' to 1
 [    89] copy
 [    8a] set column to 3
 [    8c] extended opcode 2:  set address to 0x40051a <foo+0xa>
 [    97] set 'is_stmt' to 0
 [    98] copy
 [    99] set column to 34
 [    9b] extended opcode 2:  set address to 0x40051e <foo+0xe>
 [    a6] special opcode 24: address+0 = 0x40051e <foo+0xe>, line+1 = 26
 [    a7] set column to 1
 [    a9] extended opcode 2:  set address to 0x400528 <foo+0x18>
 [    b4] extended opcode 4:  set discriminator to 1
 [    b8] special opcode 22: address+0 = 0x400528 <foo+0x18>, line-1 = 25
 [    b9] set column to 18
 [    bb] extended opcode 2:  set address to 0x40052b <foo+0x1b>
 [    c6] set file to 2
 [    c8] set 'is_stmt' to 1
 [    c9] advance line by constant -18 to 7
 [    cb] copy
 [    cc] set column to 3
 [    ce] extended opcode 2:  set address to 0x40052b <foo+0x1b>
 [    d9] special opcode 25: address+0 = 0x40052b <foo+0x1b>, line+2 = 9
 [    da] set column to 3
 [    dc] extended opcode 2:  set address to 0x40052b <foo+0x1b>
 [    e7] set 'is_stmt' to 0
 [    e8] copy
 [    e9] set column to 6
 [    eb] extended opcode 2:  set address to 0x40052f <foo+0x1f>
 [    f6] extended opcode 4:  set discriminator to 0
 [    fa] set 'is_stmt' to 1
 [    fb] special opcode 24: address+0 = 0x40052f <foo+0x1f>, line+1 = 10
 [    fc] set column to 5
 [    fe] extended opcode 2:  set address to 0x40052f <foo+0x1f>
 [   109] set 'is_stmt' to 0
 [   10a] copy
 [   10b] set column to 7
 [   10d] extended opcode 2:  set address to 0x400531 <foo+0x21>
 [   118] set 'is_stmt' to 1
 [   119] special opcode 25: address+0 = 0x400531 <foo+0x21>, line+2 = 12
 [   11a] set column to 3
 [   11c] extended opcode 2:  set address to 0x400531 <foo+0x21>
 [   127] set file to 1
 [   129] special opcode 21: address+0 = 0x400531 <foo+0x21>, line-2 = 10
 [   12a] set column to 3
 [   12c] extended opcode 2:  set address to 0x400531 <foo+0x21>
 [   137] special opcode 25: address+0 = 0x400531 <foo+0x21>, line+2 = 12
 [   138] set column to 3
 [   13a] extended opcode 2:  set address to 0x400531 <foo+0x21>
 [   145] set 'is_stmt' to 0
 [   146] copy
 [   147] set column to 6
 [   149] extended opcode 2:  set address to 0x400535 <foo+0x25>
 [   154] set 'is_stmt' to 1
 [   155] special opcode 24: address+0 = 0x400535 <foo+0x25>, line+1 = 13
 [   156] set column to 5
 [   158] extended opcode 2:  set address to 0x400535 <foo+0x25>
 [   163] set 'is_stmt' to 0
 [   164] copy
 [   165] set column to 7
 [   167] extended opcode 2:  set address to 0x400539 <foo+0x29>
 [   172] set 'is_stmt' to 1
 [   173] special opcode 25: address+0 = 0x400539 <foo+0x29>, line+2 = 15
 [   174] set column to 3
 [   176] extended opcode 2:  set address to 0x400539 <foo+0x29>
 [   181] special opcode 30: address+0 = 0x400539 <foo+0x29>, line+7 = 22
 [   182] set column to 3
 [   184] extended opcode 2:  set address to 0x400539 <foo+0x29>
 [   18f] set 'is_stmt' to 0
 [   190] copy
 [   191] set column to 6
 [   193] extended opcode 2:  set address to 0x40053d <foo+0x2d>
 [   19e] set 'is_stmt' to 1
 [   19f] special opcode 24: address+0 = 0x40053d <foo+0x2d>, line+1 = 23
 [   1a0] set column to 5
 [   1a2] extended opcode 2:  set address to 0x40053d <foo+0x2d>
 [   1ad] set 'is_stmt' to 0
 [   1ae] copy
 [   1af] set column to 12
 [   1b1] extended opcode 2:  set address to 0x400550 <baz>
 [   1bc] set 'is_stmt' to 1
 [   1bd] advance line by constant -14 to 9
 [   1bf] copy
 [   1c0] set column to 1
 [   1c2] extended opcode 2:  set address to 0x400550 <baz>
 [   1cd] special opcode 24: address+0 = 0x400550 <baz>, line+1 = 10
 [   1ce] set column to 3
 [   1d0] extended opcode 2:  set address to 0x400550 <baz>
 [   1db] special opcode 25: address+0 = 0x400550 <baz>, line+2 = 12
 [   1dc] set column to 3
 [   1de] extended opcode 2:  set address to 0x400550 <baz>
 [   1e9] set 'is_stmt' to 0
 [   1ea] copy
 [   1eb] set column to 9
 [   1ed] extended opcode 2:  set address to 0x400556 <baz+0x6>
 [   1f8] special opcode 24: address+0 = 0x400556 <baz+0x6>, line+1 = 13
 [   1f9] set column to 7
 [   1fb] extended opcode 2:  set address to 0x40055f <baz+0xf>
 [   206] set 'is_stmt' to 1
 [   207] special opcode 25: address+0 = 0x40055f <baz+0xf>, line+2 = 15
 [   208] set column to 3
 [   20a] extended opcode 2:  set address to 0x40055f <baz+0xf>
 [   215] set 'is_stmt' to 0
 [   216] copy
 [   217] set column to 7
 [   219] extended opcode 2:  set address to 0x400561
 [   224] extended opcode 1:  end of sequence

Table at offset 551:

 Length:                         441
 DWARF version:                  5
 Prologue length:                56
 Address size:                   8
 Segment selector size:          0
 Min instruction length:         1
 Max operations per instruction: 1
 Initial value if 'is_stmt':     1
 Line base:                      -10
 Line range:                     242
 Opcode base:                    13

Opcodes:
  [ 1]  0 arguments
  [ 2]  1 argument
  [ 3]  1 argument
  [ 4]  1 argument
  [ 5]  1 argument
  [ 6]  0 arguments
  [ 7]  0 arguments
  [ 8]  0 arguments
  [ 9]  1 argument
  [10]  0 arguments
  [11]  0 arguments
  [12]  1 argument

Directory table:
      [path(line_strp)]
 0     /var/tmp/hello (90)
 1     /usr/include (122)

File name table:
      [path(line_strp), directory_index(data1)]
 0     world.c (114),  0
 1     world.c (114),  0
 2     hello.h (82),  0
 3     stdlib.h (105),  1

Line number statements:
 [   26b] extended opcode 2:  set address to 0x400410 <main>
 [   276] special opcode 37: address+0 = 0x400410 <main>, line+14 = 15
 [   277] set column to 1
 [   279] extended opcode 2:  set address to 0x400410 <main>
 [   284] special opcode 24: address+0 = 0x400410 <main>, line+1 = 16
 [   285] set column to 3
 [   287] extended opcode 2:  set address to 0x400410 <main>
 [   292] special opcode 24: address+0 = 0x400410 <main>, line+1 = 17
 [   293] set column to 3
 [   295] extended opcode 2:  set address to 0x400410 <main>
 [   2a0] set 'is_stmt' to 0
 [   2a1] special opcode 21: address+0 = 0x400410 <main>, line-2 = 15
 [   2a2] set column to 1
 [   2a4] extended opcode 2:  set address to 0x400419 <main+0x9>
 [   2af] special opcode 25: address+0 = 0x400419 <main+0x9>, line+2 = 17
 [   2b0] set column to 6
 [   2b2] extended opcode 2:  set address to 0x40041e <main+0xe>
 [   2bd] set 'is_stmt' to 1
 [   2be] special opcode 24: address+0 = 0x40041e <main+0xe>, line+1 = 18
 [   2bf] set column to 5
 [   2c1] extended opcode 2:  set address to 0x40041e <main+0xe>
 [   2cc] set 'is_stmt' to 0
 [   2cd] copy
 [   2ce] set column to 7
 [   2d0] extended opcode 2:  set address to 0x400421 <main+0x11>
 [   2db] set 'is_stmt' to 1
 [   2dc] special opcode 27: address+0 = 0x400421 <main+0x11>, line+4 = 22
 [   2dd] set column to 3
 [   2df] extended opcode 2:  set address to 0x400430 <_start>
 [   2ea] extended opcode 1:  end of sequence
 [   2ed] extended opcode 2:  set address to 0x400570 <calc>
 [   2f8] special opcode 28: address+0 = 0x400570 <calc>, line+5 = 6
 [   2f9] set column to 1
 [   2fb] extended opcode 2:  set address to 0x400570 <calc>
 [   306] special opcode 24: address+0 = 0x400570 <calc>, line+1 = 7
 [   307] set column to 3
 [   309] extended opcode 2:  set address to 0x400570 <calc>
 [   314] set 'is_stmt' to 0
 [   315] copy
 [   316] set column to 6
 [   318] extended opcode 2:  set address to 0x400575 <calc+0x5>
 [   323] extended opcode 4:  set discriminator to 1
 [   327] copy
 [   328] set column to 24
 [   32a] extended opcode 2:  set address to 0x400578 <calc+0x8>
 [   335] copy
 [   336] set column to 17
 [   338] extended opcode 2:  set address to 0x40057d <calc+0xd>
 [   343] extended opcode 4:  set discriminator to 0
 [   347] set 'is_stmt' to 1
 [   348] special opcode 26: address+0 = 0x40057d <calc+0xd>, line+3 = 10
 [   349] set column to 3
 [   34b] extended opcode 2:  set address to 0x40057d <calc+0xd>
 [   356] set 'is_stmt' to 0
 [   357] copy
 [   358] set column to 10
 [   35a] extended opcode 2:  set address to 0x400583 <calc+0x13>
 [   365] set file to 2
 [   367] copy
 [   368] set column to 7
 [   36a] extended opcode 2:  set address to 0x400585 <calc+0x15>
 [   375] set file to 1
 [   377] copy
 [   378] set column to 10
 [   37a] extended opcode 2:  set address to 0x400588 <calc+0x18>
 [   385] set file to 2
 [   387] set 'is_stmt' to 1
 [   388] special opcode 20: address+0 = 0x400588 <calc+0x18>, line-3 = 7
 [   389] set column to 3
 [   38b] extended opcode 2:  set address to 0x400588 <calc+0x18>
 [   396] special opcode 25: address+0 = 0x400588 <calc+0x18>, line+2 = 9
 [   397] set column to 3
 [   399] extended opcode 2:  set address to 0x400588 <calc+0x18>
 [   3a4] set 'is_stmt' to 0
 [   3a5] special opcode 24: address+0 = 0x400588 <calc+0x18>, line+1 = 10
 [   3a6] set column to 7
 [   3a8] extended opcode 2:  set address to 0x40058f <calc+0x1f>
 [   3b3] set 'is_stmt' to 1
 [   3b4] special opcode 25: address+0 = 0x40058f <calc+0x1f>, line+2 = 12
 [   3b5] set column to 3
 [   3b7] extended opcode 2:  set address to 0x40058f <calc+0x1f>
 [   3c2] set 'is_stmt' to 0
 [   3c3] copy
 [   3c4] set column to 10
 [   3c6] extended opcode 2:  set address to 0x400598 <calc+0x28>
 [   3d1] set file to 1
 [   3d3] special opcode 22: address+0 = 0x400598 <calc+0x28>, line-1 = 11
 [   3d4] set column to 1
 [   3d6] extended opcode 2:  set address to 0x40059b
 [   3e1] extended opcode 1:  end of sequence
EOF

exit 0
