#! /bin/sh
# Copyright (C) 2015 Red Hat, Inc.
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

# - testfile-zdebug-rel.c
# #define UINT64_MAX 18446744073709551615UL
#
# int
# main (int argc, char **argv)
# {
#   unsigned long a = UINT64_MAX - 8;
#   unsigned long b = 42 + argc;
#
#   if ( a + b < b )
#     argc = a + argc;
#   else
#      b--;
#
#   return a - b;
# }
#
# gcc -Og -g -Xassembler --compress-debug-sections=none \
#     -c -o testfile-debug-rel.o testfile-zdebug-rel.c
# gcc -Og -g -Xassembler --compress-debug-sections=zlib-gnu \
#     -c -o testfile-debug-rel-g.o testfile-zdebug-rel.c
# gcc -Og -g -Xassembler --compress-debug-sections=zlib-gabi \
#     -c -o testfile-debug-rel-z.o testfile-zdebug-rel.c

testfiles testfile-debug-rel.o testfile-debug-rel-g.o testfile-debug-rel-z.o
tempfiles readelf.out
tempfiles info.out loc.out

cat > info.out << \EOF

DWARF section [ 4] '.debug_info' at offset 0x58:
 [Offset]
 Compilation unit at offset 0:
 Version: 4, Abbreviation section offset: 0, Address size: 8, Offset size: 4
 [     b]  compile_unit         abbrev: 1
           producer             (strp) "GNU C11 5.3.1 20151207 (Red Hat 5.3.1-2) -mtune=generic -march=x86-64 -g -Og"
           language             (data1) C99 (12)
           name                 (strp) "testfile-zdebug-rel.c"
           comp_dir             (strp) "/tmp"
           low_pc               (addr) 000000000000000000
           high_pc              (data8) 24 (0x0000000000000018)
           stmt_list            (sec_offset) 0
 [    2d]    subprogram           abbrev: 2
             external             (flag_present) yes
             name                 (strp) "main"
             decl_file            (data1) testfile-zdebug-rel.c (1)
             decl_line            (data1) 4
             prototyped           (flag_present) yes
             type                 (ref4) [    80]
             low_pc               (addr) 000000000000000000
             high_pc              (data8) 24 (0x0000000000000018)
             frame_base           (exprloc) 
              [ 0] call_frame_cfa
             GNU_all_call_sites   (flag_present) yes
             sibling              (ref4) [    80]
 [    4e]      formal_parameter     abbrev: 3
               name                 (strp) "argc"
               decl_file            (data1) testfile-zdebug-rel.c (1)
               decl_line            (data1) 4
               type                 (ref4) [    80]
               location             (sec_offset) location list [     0]
 [    5d]      formal_parameter     abbrev: 4
               name                 (strp) "argv"
               decl_file            (data1) testfile-zdebug-rel.c (1)
               decl_line            (data1) 4
               type                 (ref4) [    87]
               location             (exprloc) 
                [ 0] reg4
 [    6a]      variable             abbrev: 5
               name                 (string) "a"
               decl_file            (data1) testfile-zdebug-rel.c (1)
               decl_line            (data1) 6
               type                 (ref4) [    9a]
               const_value          (sdata) 18446744073709551607 (-9)
 [    74]      variable             abbrev: 6
               name                 (string) "b"
               decl_file            (data1) testfile-zdebug-rel.c (1)
               decl_line            (data1) 7
               type                 (ref4) [    9a]
               location             (exprloc) 
                [ 0] reg5
 [    80]    base_type            abbrev: 7
             byte_size            (data1) 4
             encoding             (data1) signed (5)
             name                 (string) "int"
 [    87]    pointer_type         abbrev: 8
             byte_size            (data1) 8
             type                 (ref4) [    8d]
 [    8d]    pointer_type         abbrev: 8
             byte_size            (data1) 8
             type                 (ref4) [    93]
 [    93]    base_type            abbrev: 9
             byte_size            (data1) 1
             encoding             (data1) signed_char (6)
             name                 (strp) "char"
 [    9a]    base_type            abbrev: 9
             byte_size            (data1) 8
             encoding             (data1) unsigned (7)
             name                 (strp) "long unsigned int"
EOF

cat info.out | testrun_compare ${abs_top_builddir}/src/readelf -U --debug-dump=info testfile-debug-rel.o

cat info.out | sed -e "s/'.debug_info'/'.zdebug_info'/" | testrun_compare ${abs_top_builddir}/src/readelf -U --debug-dump=info testfile-debug-rel-g.o

cat info.out | testrun_compare ${abs_top_builddir}/src/readelf -U --debug-dump=info testfile-debug-rel-z.o

cat > loc.out << \EOF

DWARF section [ 7] '.debug_loc' at offset 0x185:

 CU [     b] base: 000000000000000000
 [     0] range 0, 3
           [ 0] reg5
          range 3, 10
           [ 0] breg5 -42
           [ 2] stack_value
          range 10, 18
           [ 0] GNU_entry_value:
                [ 0] reg5
           [ 3] stack_value
EOF

cat loc.out | testrun_compare ${abs_top_builddir}/src/readelf -U --debug-dump=loc testfile-debug-rel.o

cat loc.out | sed -e "s/'.debug_loc' at offset 0x185/'.zdebug_loc' at offset 0x138/" | testrun_compare ${abs_top_builddir}/src/readelf -U --debug-dump=loc testfile-debug-rel-g.o

cat loc.out | sed -e "s/at offset 0x185/at offset 0x150/" | testrun_compare ${abs_top_builddir}/src/readelf -U --debug-dump=loc testfile-debug-rel-z.o

# Same as above, but on ppc64
testfiles testfile-debug-rel-ppc64.o
testfiles testfile-debug-rel-ppc64-g.o testfile-debug-rel-ppc64-z.o

cat > info.out << \EOF

DWARF section [ 6] '.debug_info' at offset 0x80:
 [Offset]
 Compilation unit at offset 0:
 Version: 4, Abbreviation section offset: 0, Address size: 8, Offset size: 4
 [     b]  compile_unit         abbrev: 1
           producer             (strp) "GNU C11 7.3.1 20180712 (Red Hat 7.3.1-6) -Asystem=linux -Asystem=unix -Asystem=posix -msecure-plt -g -Og"
           language             (data1) C99 (12)
           name                 (strp) "testfile-zdebug-rel.c"
           comp_dir             (strp) "/home/mjw"
           low_pc               (addr) 000000000000000000
           high_pc              (data8) 44 (0x000000000000002c)
           stmt_list            (sec_offset) 0
 [    2d]    subprogram           abbrev: 2
             external             (flag_present) yes
             name                 (strp) "main"
             decl_file            (data1) testfile-zdebug-rel.c (1)
             decl_line            (data1) 4
             prototyped           (flag_present) yes
             type                 (ref4) [    82]
             low_pc               (addr) 000000000000000000
             high_pc              (data8) 44 (0x000000000000002c)
             frame_base           (exprloc) 
              [ 0] call_frame_cfa
             GNU_all_call_sites   (flag_present) yes
             sibling              (ref4) [    82]
 [    4e]      formal_parameter     abbrev: 3
               name                 (strp) "argc"
               decl_file            (data1) testfile-zdebug-rel.c (1)
               decl_line            (data1) 4
               type                 (ref4) [    82]
               location             (sec_offset) location list [     0]
 [    5d]      formal_parameter     abbrev: 4
               name                 (strp) "argv"
               decl_file            (data1) testfile-zdebug-rel.c (1)
               decl_line            (data1) 4
               type                 (ref4) [    89]
               location             (exprloc) 
                [ 0] reg4
 [    6a]      variable             abbrev: 5
               name                 (string) "a"
               decl_file            (data1) testfile-zdebug-rel.c (1)
               decl_line            (data1) 6
               type                 (ref4) [    9c]
               const_value          (sdata) 18446744073709551607 (-9)
 [    74]      variable             abbrev: 6
               name                 (string) "b"
               decl_file            (data1) testfile-zdebug-rel.c (1)
               decl_line            (data1) 7
               type                 (ref4) [    9c]
               location             (sec_offset) location list [    4e]
 [    82]    base_type            abbrev: 7
             byte_size            (data1) 4
             encoding             (data1) signed (5)
             name                 (string) "int"
 [    89]    pointer_type         abbrev: 8
             byte_size            (data1) 8
             type                 (ref4) [    8f]
 [    8f]    pointer_type         abbrev: 8
             byte_size            (data1) 8
             type                 (ref4) [    95]
 [    95]    base_type            abbrev: 9
             byte_size            (data1) 1
             encoding             (data1) unsigned_char (8)
             name                 (strp) "char"
 [    9c]    base_type            abbrev: 9
             byte_size            (data1) 8
             encoding             (data1) unsigned (7)
             name                 (strp) "long unsigned int"
EOF

cat info.out | testrun_compare ${abs_top_builddir}/src/readelf -U --debug-dump=info testfile-debug-rel-ppc64.o

cat info.out | sed -e "s/'.debug_info'/'.zdebug_info'/" | testrun_compare ${abs_top_builddir}/src/readelf -U --debug-dump=info testfile-debug-rel-ppc64-g.o

cat info.out | testrun_compare ${abs_top_builddir}/src/readelf -U --debug-dump=info testfile-debug-rel-ppc64-z.o

cat > loc.out << \EOF

DWARF section [ 9] '.debug_loc' at offset 0x1af:

 CU [     b] base: 000000000000000000
 [     0] range 0, 4
           [ 0] reg3
          range 4, 14
           [ 0] breg3 -42
           [ 2] stack_value
          range 14, 2c
           [ 0] GNU_entry_value:
                [ 0] reg3
           [ 3] stack_value
 [    4e] range 8, 18
           [ 0] reg3
EOF

cat loc.out | testrun_compare ${abs_top_builddir}/src/readelf -U --debug-dump=loc testfile-debug-rel-ppc64.o

cat loc.out | sed -e "s/'.debug_loc' at offset 0x1af/'.zdebug_loc' at offset 0x15f/" | testrun_compare ${abs_top_builddir}/src/readelf -U --debug-dump=loc testfile-debug-rel-ppc64-g.o

cat loc.out | sed -e "s/at offset 0x1af/at offset 0x177/" | testrun_compare ${abs_top_builddir}/src/readelf -U --debug-dump=loc testfile-debug-rel-ppc64-z.o

exit 0
