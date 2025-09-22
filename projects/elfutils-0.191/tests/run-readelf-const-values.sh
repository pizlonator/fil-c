#! /bin/sh
# Test for displaying DW_AT_const_types with the "correct" sign.
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

# = s.c
#
# int s()
# {
#   int i = -1;
#   int j = -1;
#
#   return i - j;
# }
#
# = m.c
#
# extern int s();
#
# int
# main ()
# {
#   const signed char sc = -2;
#   const unsigned char uc = 254;
#
#   const signed short ss = -16;
#   const unsigned short us = 65520;
#
#   const signed int si = -3;
#   const unsigned int ui = 4200000000;
#
#   signed long sl = -1;
#   unsigned long ul = 0xffffffffffffffffUL;
#
#   return s ();
# }
#
# gcc -gdwarf-5 -O2 -c s.c
# gcc -gdwarf-4 -O2 -c m.c
# gcc -o testfile-const-values s.o m.o
# eu-strip -g -f testfile-const-values.debug testfile-const-values

testfiles testfile-const-values.debug

testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=info testfile-const-values.debug << EOF

DWARF section [28] '.debug_info' at offset 0x2e0:
 [Offset]
 Compilation unit at offset 0:
 Version: 5, Abbreviation section offset: 0, Address size: 8, Offset size: 4
 Unit type: compile (1)
 [     c]  compile_unit         abbrev: 2
           producer             (strp) "GNU C11 7.3.1 20180303 (Red Hat 7.3.1-5) -mtune=generic -march=x86-64 -gdwarf-5 -O2"
           language             (data1) C11 (29)
           name                 (string) "s.c"
           comp_dir             (strp) "/home/mark/build/elfutils-obj"
           low_pc               (addr) 0x00000000004004d0
           high_pc              (data8) 3 (0x00000000004004d3)
           stmt_list            (sec_offset) 0
 [    2e]    subprogram           abbrev: 3
             external             (flag_present) yes
             name                 (string) "s"
             decl_file            (data1) s.c (1)
             decl_line            (data1) 1
             type                 (ref4) [    5e]
             low_pc               (addr) 0x00000000004004d0
             high_pc              (data8) 3 (0x00000000004004d3)
             frame_base           (exprloc) 
              [ 0] call_frame_cfa
             call_all_calls       (flag_present) yes
             sibling              (ref4) [    5e]
 [    4d]      variable             abbrev: 1
               name                 (string) "i"
               decl_file            (implicit_const) s.c (1)
               decl_line            (data1) 3
               type                 (ref4) [    5e]
               const_value          (implicit_const) -1
 [    55]      variable             abbrev: 1
               name                 (string) "j"
               decl_file            (implicit_const) s.c (1)
               decl_line            (data1) 4
               type                 (ref4) [    5e]
               const_value          (implicit_const) -1
 [    5e]    base_type            abbrev: 4
             byte_size            (data1) 4
             encoding             (data1) signed (5)
             name                 (string) "int"
 Compilation unit at offset 102:
 Version: 4, Abbreviation section offset: 73, Address size: 8, Offset size: 4
 [    71]  compile_unit         abbrev: 1
           producer             (strp) "GNU C11 7.3.1 20180303 (Red Hat 7.3.1-5) -mtune=generic -march=x86-64 -gdwarf-4 -O2"
           language             (data1) C99 (12)
           name                 (string) "m.c"
           comp_dir             (strp) "/home/mark/build/elfutils-obj"
           ranges               (sec_offset) range list [     0]
           low_pc               (addr) 000000000000000000
           stmt_list            (sec_offset) 54
 [    8f]    subprogram           abbrev: 2
             external             (flag_present) yes
             name                 (strp) "main"
             decl_file            (data1) m.c (1)
             decl_line            (data1) 4
             type                 (ref4) [   119]
             low_pc               (addr) 0x00000000004003e0
             high_pc              (data8) 7 (0x00000000004003e7)
             frame_base           (exprloc) 
              [ 0] call_frame_cfa
             GNU_all_call_sites   (flag_present) yes
             sibling              (ref4) [   119]
 [    b0]      variable             abbrev: 3
               name                 (string) "sc"
               decl_file            (data1) m.c (1)
               decl_line            (data1) 6
               type                 (ref4) [   12c]
               const_value          (sdata) -2
 [    bb]      variable             abbrev: 3
               name                 (string) "uc"
               decl_file            (data1) m.c (1)
               decl_line            (data1) 7
               type                 (ref4) [   138]
               const_value          (sdata) 254 (-2)
 [    c6]      variable             abbrev: 3
               name                 (string) "ss"
               decl_file            (data1) m.c (1)
               decl_line            (data1) 9
               type                 (ref4) [   144]
               const_value          (sdata) -16
 [    d1]      variable             abbrev: 3
               name                 (string) "us"
               decl_file            (data1) m.c (1)
               decl_line            (data1) 10
               type                 (ref4) [   150]
               const_value          (sdata) 65520 (-16)
 [    dc]      variable             abbrev: 3
               name                 (string) "si"
               decl_file            (data1) m.c (1)
               decl_line            (data1) 12
               type                 (ref4) [   120]
               const_value          (sdata) -3
 [    e7]      variable             abbrev: 3
               name                 (string) "ui"
               decl_file            (data1) m.c (1)
               decl_line            (data1) 13
               type                 (ref4) [   15c]
               const_value          (sdata) 4200000000 (-94967296)
 [    f5]      variable             abbrev: 3
               name                 (string) "sl"
               decl_file            (data1) m.c (1)
               decl_line            (data1) 15
               type                 (ref4) [   161]
               const_value          (sdata) -1
 [   100]      variable             abbrev: 3
               name                 (string) "ul"
               decl_file            (data1) m.c (1)
               decl_line            (data1) 16
               type                 (ref4) [   168]
               const_value          (sdata) 18446744073709551615 (-1)
 [   10b]      GNU_call_site        abbrev: 4
               low_pc               (addr) 0x00000000004003e7
               GNU_tail_call        (flag_present) yes
               abstract_origin      (ref4) [   16f]
 [   119]    base_type            abbrev: 5
             byte_size            (data1) 4
             encoding             (data1) signed (5)
             name                 (string) "int"
 [   120]    const_type           abbrev: 6
             type                 (ref4) [   119]
 [   125]    base_type            abbrev: 7
             byte_size            (data1) 1
             encoding             (data1) signed_char (6)
             name                 (strp) "signed char"
 [   12c]    const_type           abbrev: 6
             type                 (ref4) [   125]
 [   131]    base_type            abbrev: 7
             byte_size            (data1) 1
             encoding             (data1) unsigned_char (8)
             name                 (strp) "unsigned char"
 [   138]    const_type           abbrev: 6
             type                 (ref4) [   131]
 [   13d]    base_type            abbrev: 7
             byte_size            (data1) 2
             encoding             (data1) signed (5)
             name                 (strp) "short int"
 [   144]    const_type           abbrev: 6
             type                 (ref4) [   13d]
 [   149]    base_type            abbrev: 7
             byte_size            (data1) 2
             encoding             (data1) unsigned (7)
             name                 (strp) "short unsigned int"
 [   150]    const_type           abbrev: 6
             type                 (ref4) [   149]
 [   155]    base_type            abbrev: 7
             byte_size            (data1) 4
             encoding             (data1) unsigned (7)
             name                 (strp) "unsigned int"
 [   15c]    const_type           abbrev: 6
             type                 (ref4) [   155]
 [   161]    base_type            abbrev: 7
             byte_size            (data1) 8
             encoding             (data1) signed (5)
             name                 (strp) "long int"
 [   168]    base_type            abbrev: 7
             byte_size            (data1) 8
             encoding             (data1) unsigned (7)
             name                 (strp) "long unsigned int"
 [   16f]    subprogram           abbrev: 8
             external             (flag_present) yes
             declaration          (flag_present) yes
             linkage_name         (string) "s"
             name                 (string) "s"
             decl_file            (data1) m.c (1)
             decl_line            (data1) 1
EOF

exit 0
