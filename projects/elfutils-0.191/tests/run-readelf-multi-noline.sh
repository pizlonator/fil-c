#! /bin/sh
# Copyright (C) 2021 Red Hat, Inc.
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

# = a.c =
# int a;

# = b.c =
# int b;

# = m.c =
# int main () { }

# gcc -g -o testfile_multi_noline a.c b.c m.c

testfiles testfile_multi_noline

testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=line testfile_multi_noline <<\EOF

DWARF section [29] '.debug_line' at offset 0x1221:

Table at offset 0:

 Length:                         32
 DWARF version:                  3
 Prologue length:                26
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
 1     0     0         0         a.c

No line number statements.

Table at offset 36:

 Length:                         32
 DWARF version:                  3
 Prologue length:                26
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
 1     0     0         0         b.c

No line number statements.

Table at offset 72:

 Length:                         54
 DWARF version:                  3
 Prologue length:                26
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
 1     0     0         0         m.c

Line number statements:
 [    6c] set column to 13
 [    6e] extended opcode 2:  set address to +0x724 <main>
 [    79] copy
 [    7a] set column to 15
 [    7c] special opcode 32: address+4 = +0x728 <main+0x4>, line+0 = 1
 [    7d] advance address by 4 to +0x72c
 [    7f] extended opcode 1:  end of sequence
EOF

testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=decodedline testfile_multi_noline <<\EOF

DWARF section [29] '.debug_line' at offset 0x1221:

 CU [b] a.c
  line:col SBPE* disc isa op address (Statement Block Prologue Epilogue *End)
 CU [44] b.c
  line:col SBPE* disc isa op address (Statement Block Prologue Epilogue *End)
 CU [7d] m.c
  line:col SBPE* disc isa op address (Statement Block Prologue Epilogue *End)
  /tmp/m.c (mtime: 0, length: 0)
     1:13  S        0   0  0 +0x0000000000000724 <main>
     1:15  S        0   0  0 +0x0000000000000728 <main+0x4>
     1:15  S   *    0   0  0 +0x000000000000072b <main+0x7>

EOF

exit 0
