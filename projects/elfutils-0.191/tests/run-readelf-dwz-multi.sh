#! /bin/sh
# Copyright (C) 2012, 2013 Red Hat, Inc.
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
#
# #include <stdio.h>
#
# struct foobar
# {
#   int foo;
#   struct foobar *bar;
# };
#
# extern int call_foo(struct foobar *foobar_struct_ptr);

# main.c
#
# #include "common.h"
#
# int main(int argc, char ** argv)
# {
#   struct foobar b;
#   b.foo = 42;
#   b.bar = &b;
#
#   return call_foo(b.bar);
# }

# shared.c
#
# #include "common.h"
#
# int call_foo(struct foobar *fb)
# {
#   return fb->bar->foo - 42;
# }

# gcc -fPIC -g -c -Wall shared.c
# gcc -shared -o libtestfile_multi_shared.so shared.o
# gcc -g -o testfile_multi_main -L. -ltestfile_multi_shared main.c -Wl,-rpath,.
# dwz -m testfile_multi.dwz testfile_multi_main libtestfile_multi_shared.so

# main.c
#
# struct foobarbaz
# {
#   int counter;
#   char *bookstore;
# };
#
# int
# main (int argc, char **argv)
# {
#   struct foobarbaz fbb;
#   return 0;
# }

# gcc -g -o testfile-dwzstr main.c
# cp testfile-dwzstr testfile-dwzstr.alt
# dwz -m testfile-dwzstr.multi testfile-dwzstr testfile-dwzstr.alt

testfiles libtestfile_multi_shared.so testfile_multi_main testfile_multi.dwz
testfiles testfile-dwzstr testfile-dwzstr.multi

testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=info testfile_multi_main <<\EOF

DWARF section [28] '.debug_info' at offset 0x1078:
 [Offset]
 Compilation unit at offset 0:
 Version: 4, Abbreviation section offset: 0, Address size: 8, Offset size: 4
 [     b]  compile_unit         abbrev: 6
           producer             (strp) "GNU C 4.7.0 20120507 (Red Hat 4.7.0-5) -mtune=generic -march=x86-64 -g"
           language             (data1) C89 (1)
           name                 (strp) "main.c"
           comp_dir             (GNU_strp_alt) "/home/mark/src/tests/dwz"
           low_pc               (addr) 0x00000000004006ac <main>
           high_pc              (udata) 44 (0x00000000004006d8)
           stmt_list            (sec_offset) 0
 [    26]    imported_unit        abbrev: 5
             import               (GNU_ref_alt) [     b]
 [    2b]    pointer_type         abbrev: 1
             byte_size            (data1) 8
             type                 (GNU_ref_alt) [    53]
 [    31]    subprogram           abbrev: 3
             external             (flag_present) yes
             name                 (strp) "main"
             decl_file            (data1) main.c (1)
             decl_line            (data1) 3
             prototyped           (flag_present) yes
             type                 (GNU_ref_alt) [    3e]
             low_pc               (addr) 0x00000000004006ac <main>
             high_pc              (udata) 44 (0x00000000004006d8)
             frame_base           (exprloc) 
              [ 0] call_frame_cfa
             GNU_all_tail_call_sites (flag_present) yes
             sibling              (ref_udata) [    6e]
 [    48]      formal_parameter     abbrev: 8
               name                 (strp) "argc"
               decl_file            (data1) main.c (1)
               decl_line            (data1) 3
               type                 (GNU_ref_alt) [    3e]
               location             (exprloc) 
                [ 0] fbreg -36
 [    56]      formal_parameter     abbrev: 4
               name                 (strp) "argv"
               decl_file            (data1) main.c (1)
               decl_line            (data1) 3
               type                 (ref_udata) [    6e]
               location             (exprloc) 
                [ 0] fbreg -48
 [    61]      variable             abbrev: 7
               name                 (string) "b"
               decl_file            (data1) main.c (1)
               decl_line            (data1) 5
               type                 (GNU_ref_alt) [    5a]
               location             (exprloc) 
                [ 0] fbreg -32
 [    6e]    pointer_type         abbrev: 2
             byte_size            (data1) 8
             type                 (ref_udata) [    2b]
EOF

# Same as above, but find alt debug file in a .dwz subdir.
mkdir .dwz
mv testfile_multi.dwz .dwz
testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=info testfile_multi_main <<\EOF

DWARF section [28] '.debug_info' at offset 0x1078:
 [Offset]
 Compilation unit at offset 0:
 Version: 4, Abbreviation section offset: 0, Address size: 8, Offset size: 4
 [     b]  compile_unit         abbrev: 6
           producer             (strp) "GNU C 4.7.0 20120507 (Red Hat 4.7.0-5) -mtune=generic -march=x86-64 -g"
           language             (data1) C89 (1)
           name                 (strp) "main.c"
           comp_dir             (GNU_strp_alt) "/home/mark/src/tests/dwz"
           low_pc               (addr) 0x00000000004006ac <main>
           high_pc              (udata) 44 (0x00000000004006d8)
           stmt_list            (sec_offset) 0
 [    26]    imported_unit        abbrev: 5
             import               (GNU_ref_alt) [     b]
 [    2b]    pointer_type         abbrev: 1
             byte_size            (data1) 8
             type                 (GNU_ref_alt) [    53]
 [    31]    subprogram           abbrev: 3
             external             (flag_present) yes
             name                 (strp) "main"
             decl_file            (data1) main.c (1)
             decl_line            (data1) 3
             prototyped           (flag_present) yes
             type                 (GNU_ref_alt) [    3e]
             low_pc               (addr) 0x00000000004006ac <main>
             high_pc              (udata) 44 (0x00000000004006d8)
             frame_base           (exprloc) 
              [ 0] call_frame_cfa
             GNU_all_tail_call_sites (flag_present) yes
             sibling              (ref_udata) [    6e]
 [    48]      formal_parameter     abbrev: 8
               name                 (strp) "argc"
               decl_file            (data1) main.c (1)
               decl_line            (data1) 3
               type                 (GNU_ref_alt) [    3e]
               location             (exprloc) 
                [ 0] fbreg -36
 [    56]      formal_parameter     abbrev: 4
               name                 (strp) "argv"
               decl_file            (data1) main.c (1)
               decl_line            (data1) 3
               type                 (ref_udata) [    6e]
               location             (exprloc) 
                [ 0] fbreg -48
 [    61]      variable             abbrev: 7
               name                 (string) "b"
               decl_file            (data1) main.c (1)
               decl_line            (data1) 5
               type                 (GNU_ref_alt) [    5a]
               location             (exprloc) 
                [ 0] fbreg -32
 [    6e]    pointer_type         abbrev: 2
             byte_size            (data1) 8
             type                 (ref_udata) [    2b]
EOF
mv .dwz/testfile_multi.dwz .
rmdir .dwz

testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=info libtestfile_multi_shared.so <<\EOF

DWARF section [25] '.debug_info' at offset 0x106c:
 [Offset]
 Compilation unit at offset 0:
 Version: 4, Abbreviation section offset: 0, Address size: 8, Offset size: 4
 [     b]  compile_unit         abbrev: 3
           producer             (strp) "GNU C 4.7.0 20120507 (Red Hat 4.7.0-5) -fpreprocessed -mtune=generic -march=x86-64 -g -fPIC"
           language             (data1) C89 (1)
           name                 (strp) "shared.c"
           comp_dir             (GNU_strp_alt) "/home/mark/src/tests/dwz"
           low_pc               (addr) +0x0000000000000670 <call_foo>
           high_pc              (udata) 23 (+0x0000000000000687)
           stmt_list            (sec_offset) 0
 [    26]    imported_unit        abbrev: 2
             import               (GNU_ref_alt) [     b]
 [    2b]    subprogram           abbrev: 1
             external             (flag_present) yes
             name                 (strp) "call_foo"
             decl_file            (data1) shared.c (1)
             decl_line            (data1) 3
             prototyped           (flag_present) yes
             type                 (GNU_ref_alt) [    3e]
             low_pc               (addr) +0x0000000000000670 <call_foo>
             high_pc              (udata) 23 (+0x0000000000000687)
             frame_base           (exprloc) 
              [ 0] call_frame_cfa
             GNU_all_call_sites   (flag_present) yes
 [    41]      formal_parameter     abbrev: 4
               name                 (string) "fb"
               decl_file            (data1) shared.c (1)
               decl_line            (data1) 3
               type                 (GNU_ref_alt) [    76]
               location             (exprloc) 
                [ 0] fbreg -24
EOF

# Same as above, but find alt debug file in a .dwz subdir.
mkdir .dwz
mv testfile_multi.dwz .dwz
testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=info libtestfile_multi_shared.so <<\EOF

DWARF section [25] '.debug_info' at offset 0x106c:
 [Offset]
 Compilation unit at offset 0:
 Version: 4, Abbreviation section offset: 0, Address size: 8, Offset size: 4
 [     b]  compile_unit         abbrev: 3
           producer             (strp) "GNU C 4.7.0 20120507 (Red Hat 4.7.0-5) -fpreprocessed -mtune=generic -march=x86-64 -g -fPIC"
           language             (data1) C89 (1)
           name                 (strp) "shared.c"
           comp_dir             (GNU_strp_alt) "/home/mark/src/tests/dwz"
           low_pc               (addr) +0x0000000000000670 <call_foo>
           high_pc              (udata) 23 (+0x0000000000000687)
           stmt_list            (sec_offset) 0
 [    26]    imported_unit        abbrev: 2
             import               (GNU_ref_alt) [     b]
 [    2b]    subprogram           abbrev: 1
             external             (flag_present) yes
             name                 (strp) "call_foo"
             decl_file            (data1) shared.c (1)
             decl_line            (data1) 3
             prototyped           (flag_present) yes
             type                 (GNU_ref_alt) [    3e]
             low_pc               (addr) +0x0000000000000670 <call_foo>
             high_pc              (udata) 23 (+0x0000000000000687)
             frame_base           (exprloc) 
              [ 0] call_frame_cfa
             GNU_all_call_sites   (flag_present) yes
 [    41]      formal_parameter     abbrev: 4
               name                 (string) "fb"
               decl_file            (data1) shared.c (1)
               decl_line            (data1) 3
               type                 (GNU_ref_alt) [    76]
               location             (exprloc) 
                [ 0] fbreg -24
EOF
mv .dwz/testfile_multi.dwz .
rmdir .dwz

testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=info testfile-dwzstr <<\EOF

DWARF section [28] '.debug_info' at offset 0x1088:
 [Offset]
 Compilation unit at offset 0:
 Version: 4, Abbreviation section offset: 0, Address size: 8, Offset size: 4
 [     b]  compile_unit         abbrev: 5
           producer             (GNU_strp_alt) "GNU C 4.7.2 20121109 (Red Hat 4.7.2-8) -mtune=generic -march=x86-64 -g"
           language             (data1) C89 (1)
           name                 (GNU_strp_alt) "main.c"
           comp_dir             (GNU_strp_alt) "/home/mark/src/tests"
           low_pc               (addr) 0x00000000004004ec <main>
           high_pc              (udata) 18 (0x00000000004004fe)
           stmt_list            (sec_offset) 0
 [    26]    imported_unit        abbrev: 2
             import               (GNU_ref_alt) [     b]
 [    2b]    subprogram           abbrev: 4
             external             (flag_present) yes
             name                 (GNU_strp_alt) "main"
             decl_file            (data1) main.c (1)
             decl_line            (data1) 8
             prototyped           (flag_present) yes
             type                 (GNU_ref_alt) [    30]
             low_pc               (addr) 0x00000000004004ec <main>
             high_pc              (udata) 18 (0x00000000004004fe)
             frame_base           (exprloc) 
              [ 0] call_frame_cfa
             GNU_all_call_sites   (flag_present) yes
 [    41]      formal_parameter     abbrev: 1
               name                 (GNU_strp_alt) "argc"
               decl_file            (data1) main.c (1)
               decl_line            (data1) 8
               type                 (GNU_ref_alt) [    30]
               location             (exprloc) 
                [ 0] fbreg -36
 [    4f]      formal_parameter     abbrev: 1
               name                 (GNU_strp_alt) "argv"
               decl_file            (data1) main.c (1)
               decl_line            (data1) 8
               type                 (GNU_ref_alt) [    41]
               location             (exprloc) 
                [ 0] fbreg -48
 [    5d]      variable             abbrev: 3
               name                 (string) "fbb"
               decl_file            (data1) main.c (1)
               decl_line            (data1) 10
               type                 (GNU_ref_alt) [    14]
               location             (exprloc) 
                [ 0] fbreg -32
EOF

exit 0
