#! /bin/sh
# Copyright (C) 2012 Red Hat, Inc.
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

# common.h
# struct foo
# {
#   const char *bar;
# };
#
# extern char *global;
# int say (struct foo *prefix);

# hello.c
# #include "common.h"
#
# static char *hello = "Hello";
#
# int
# main (int argc, char **argv)
# {
#   struct foo baz;
#   global = hello;
#   baz.bar = global;
#   return say(&baz);
# }

# world.c
# #include "common.h"
#
# char *global;
#
# static int hello (const char *bar)
# {
#   return bar == global;
# }
#
# int
# say (struct foo *prefix)
# {
#   return hello (prefix->bar);
# }

# gcc -g -fdebug-types-section -c hello.c
# gcc -g -fdebug-types-section -c world.c
# gcc -g -fdebug-types-section -o testfilegdbindex7 hello.o world.o
# gdb testfilegdbindex7
# (gdb) save gdb-index .
# objcopy --add-section .gdb_index=testfilegdbindex7.gdb-index --set-section-flags .gdb_index=readonly testfilegdbindex7 testfilegdbindex7

testfiles testfilegdbindex5 testfilegdbindex7 testfilegdbindex9 testfilegdbindex9-no-maininfo

testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=gdb_index testfilegdbindex5 <<\EOF

GDB section [33] '.gdb_index' at offset 0xe76 contains 8383 bytes :
 Version:         5
 CU offset:       0x18
 TU offset:       0x38
 address offset:  0x50
 symbol offset:   0x78
 constant offset: 0x2078

 CU list at offset 0x18 contains 2 entries:
 [   0] start: 00000000, length:   184
 [   1] start: 0x0000b8, length:   204

 TU list at offset 0x38 contains 1 entries:
 [   0] CU offset:     0, type offset:    29, signature: 0x87e03f92cc37cdf0

 Address list at offset 0x50 contains 2 entries:
 [   0] 0x000000000040049c <main>..0x00000000004004d1 <main+0x35>, CU index:     0
 [   1] 0x00000000004004d4 <hello>..0x000000000040050b <say+0x1c>, CU index:     1

 Symbol table at offset 0x50 contains 1024 slots:
 [ 123] symbol: global, CUs: 1
 [ 489] symbol: main, CUs: 0
 [ 518] symbol: char, CUs: 0
 [ 661] symbol: foo, CUs: 0T
 [ 741] symbol: hello, CUs: 0, 1
 [ 746] symbol: say, CUs: 1
 [ 754] symbol: int, CUs: 0
EOF

testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=gdb_index testfilegdbindex7 <<\EOF

GDB section [33] '.gdb_index' at offset 0xe76 contains 8399 bytes :
 Version:         7
 CU offset:       0x18
 TU offset:       0x38
 address offset:  0x50
 symbol offset:   0x78
 constant offset: 0x2078

 CU list at offset 0x18 contains 2 entries:
 [   0] start: 00000000, length:   184
 [   1] start: 0x0000b8, length:   204

 TU list at offset 0x38 contains 1 entries:
 [   0] CU offset:     0, type offset:    29, signature: 0x87e03f92cc37cdf0

 Address list at offset 0x50 contains 2 entries:
 [   0] 0x000000000040049c <main>..0x00000000004004d1 <main+0x35>, CU index:     0
 [   1] 0x00000000004004d4 <hello>..0x000000000040050b <say+0x1c>, CU index:     1

 Symbol table at offset 0x50 contains 1024 slots:
 [ 123] symbol: global, CUs: 1 (var:G)
 [ 489] symbol: main, CUs: 0 (func:G)
 [ 518] symbol: char, CUs: 0 (type:S)
 [ 661] symbol: foo, CUs: 0T (type:S)
 [ 741] symbol: hello, CUs: 0 (var:S), 1 (func:S)
 [ 746] symbol: say, CUs: 1 (func:G)
 [ 754] symbol: int, CUs: 0 (type:S)
EOF

# testfilegdbindex9-no-maininfo is built the same way as testfilegdbindex7.
testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=gdb_index testfilegdbindex9-no-maininfo <<\EOF

GDB section [33] '.gdb_index' at offset 0x38e1 contains 8415 bytes :
 Version:         9
 CU offset:       0x1c
 TU offset:       0x3c
 address offset:  0x54
 symbol offset:   0x7c
 shortcut offset: 0x207c
 constant offset: 0x2084

 CU list at offset 0x1c contains 2 entries:
 [   0] start: 0x00004c, length:   220
 [   1] start: 0x000128, length:   214

 TU list at offset 0x3c contains 1 entries:
 [   0] CU offset:     0, type offset:    30, signature: 0x87e03f92cc37cdf0

 Address list at offset 0x54 contains 2 entries:
 [   0] 0x0000000000401106 <main>..0x000000000040113b <main+0x35>, CU index:     1
 [   1] 0x000000000040113c <hello>..0x0000000000401173 <say+0x1c>, CU index:     2

 Symbol table at offset 0x54 contains 1024 slots:
 [ 123] symbol: global, CUs: 1 (var:G), 0T (var:G)
 [ 489] symbol: main, CUs: 1 (func:G)
 [ 518] symbol: char, CUs: 0 (type:S)
 [ 661] symbol: foo, CUs: 0 (type:S)
 [ 741] symbol: hello, CUs: 1 (var:S), 0T (func:S)
 [ 746] symbol: say, CUs: 0T (func:G)
 [ 754] symbol: int, CUs: 1 (type:S)

Shortcut table at offset 0x207c contains 2 slots:
Language of main: ??? (0)
Name of main: <unknown>
EOF

# testfilegdbindex9.f90
#
# program repro
#  type small_stride
#     character*40 long_string
#     integer      small_pad
#  end type small_stride
#  type(small_stride), dimension (20), target :: unpleasant
#  character*40, pointer, dimension(:):: c40pt
#  integer i
#  do i = 0,19
#     unpleasant(i+1)%small_pad = i+1
#     unpleasant(i+1)%long_string = char (ichar('0') + i)
#  end do
#  c40pt => unpleasant%long_string
#  print *, c40pt
#end program repro

# gfortran -g -o testfilegdbindex9 testfilegdbindex9.f90
# gdb-add-index testfilegdbindex9

testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=gdb_index testfilegdbindex9 <<\EOF

GDB section [35] '.gdb_index' at offset 0x37d9 contains 8395 bytes :
 Version:         9
 CU offset:       0x1c
 TU offset:       0x2c
 address offset:  0x2c
 symbol offset:   0x40
 shortcut offset: 0x2040
 constant offset: 0x2048

 CU list at offset 0x1c contains 1 entries:
 [   0] start: 00000000, length:   307

 TU list at offset 0x2c contains 0 entries:

 Address list at offset 0x2c contains 1 entries:
 [   0] 0x0000000000401166 <MAIN__>..0x00000000004013f0 <main+0x3a>, CU index:     0

 Symbol table at offset 0x2c contains 1024 slots:
 [  61] symbol: small_stride, CUs: 0 (type:S)
 [  71] symbol: integer(kind=8), CUs: 0 (type:S)
 [ 161] symbol: character(kind=1), CUs: 0 (type:S)
 [ 397] symbol: unpleasant, CUs: 0 (var:S)
 [ 489] symbol: main, CUs: 0 (func:G)
 [ 827] symbol: integer(kind=4), CUs: 0 (type:S)
 [ 858] symbol: c40pt, CUs: 0 (var:S)
 [ 965] symbol: repro, CUs: 0 (func:S)
 [1016] symbol: i, CUs: 0 (var:S)

Shortcut table at offset 0x2040 contains 2 slots:
Language of main: Fortran08
Name of main: repro
EOF

exit 0
