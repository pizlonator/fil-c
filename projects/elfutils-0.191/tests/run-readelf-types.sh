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

# Make sure --debug-dump=info implies .debug_types, even when implicit.
# See run-typeiter.sh
testfiles testfile-debug-types

testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=str --debug-dump=info testfile-debug-types<<\EOF

DWARF section [28] '.debug_info' at offset 0x1089:
 [Offset]
 Compilation unit at offset 0:
 Version: 4, Abbreviation section offset: 0, Address size: 8, Offset size: 4
 [     b]  compile_unit         abbrev: 8
           producer             (strp) "GNU C++ 4.8.2 20140120 (Red Hat 4.8.2-16) -mtune=generic -march=x86-64 -g -fdebug-types-section"
           language             (data1) C_plus_plus (4)
           comp_dir             (strp) "/home/mark/src/elfutils/tests"
           low_pc               (addr) 0x00000000004005b0 <main>
           high_pc              (data8) 11 (0x00000000004005bb)
           stmt_list            (sec_offset) 0
 [    29]    subprogram           abbrev: 9
             external             (flag_present) yes
             name                 (strp) "main"
             decl_file            (data1) <stdin> (1)
             decl_line            (data1) 1
             type                 (ref4) [    46]
             low_pc               (addr) 0x00000000004005b0 <main>
             high_pc              (data8) 11 (0x00000000004005bb)
             frame_base           (exprloc) 
              [ 0] call_frame_cfa
             GNU_all_call_sites   (flag_present) yes
 [    46]    base_type            abbrev: 10
             byte_size            (data1) 4
             encoding             (data1) signed (5)
             name                 (string) "int"
 [    4d]    variable             abbrev: 11
             name                 (string) "a"
             decl_file            (data1) <stdin> (1)
             decl_line            (data1) 1
             type                 (ref_sig8) {18763953736e2de0}
             external             (flag_present) yes
             location             (exprloc) 
              [ 0] addr 0x601030 <a>
 [    64]    variable             abbrev: 11
             name                 (string) "b"
             decl_file            (data1) <stdin> (1)
             decl_line            (data1) 1
             type                 (ref_sig8) {7cf9bbf793fcaf13}
             external             (flag_present) yes
             location             (exprloc) 
              [ 0] addr 0x601031 <b>

DWARF section [31] '.debug_str' at offset 0x11dd:
 Offset  String
 [   0]  "/home/mark/src/elfutils/tests"
 [  1e]  "GNU C++ 4.8.2 20140120 (Red Hat 4.8.2-16) -mtune=generic -march=x86-64 -g -fdebug-types-section"
 [  7e]  "main"

DWARF section [32] '.debug_types' at offset 0x1260:
 [Offset]
 Type unit at offset 0:
 Version: 4, Abbreviation section offset: 0, Address size: 8, Offset size: 4
 Type signature: 0x7cf9bbf793fcaf13, Type offset: 0x38 [38]
 [    17]  type_unit            abbrev: 1
           language             (data1) C_plus_plus (4)
           GNU_odr_signature    (data8) 4783233826607187165
           stmt_list            (sec_offset) 0
 [    25]    structure_type       abbrev: 2
             name                 (string) "A"
             signature            (ref_sig8) {18763953736e2de0}
             declaration          (flag_present) yes
             sibling              (ref4) [    38]
 [    34]      structure_type       abbrev: 3
               name                 (string) "B"
               declaration          (flag_present) yes
 [    38]    structure_type       abbrev: 4
             name                 (string) "B"
             byte_size            (data1) 1
             decl_file            (data1) <stdin> (1)
             decl_line            (data1) 1
             specification        (ref4) [    34]
 Type unit at offset 67:
 Version: 4, Abbreviation section offset: 0, Address size: 8, Offset size: 4
 Type signature: 0x18763953736e2de0, Type offset: 0x25 [68]
 [    5a]  type_unit            abbrev: 1
           language             (data1) C_plus_plus (4)
           GNU_odr_signature    (data8) 16005269134005989797
           stmt_list            (sec_offset) 0
 [    68]    structure_type       abbrev: 5
             name                 (string) "A"
             byte_size            (data1) 1
             decl_file            (data1) <stdin> (1)
             decl_line            (data1) 1
 [    6e]      structure_type       abbrev: 6
               name                 (string) "B"
               declaration          (flag_present) yes
               signature            (ref_sig8) {7cf9bbf793fcaf13}
 [    79]      member               abbrev: 7
               name                 (string) "x"
               decl_file            (data1) <stdin> (1)
               decl_line            (data1) 1
               type                 (ref4) [    6e]
               data_member_location (data1) 0
EOF

exit 0
