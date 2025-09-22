#! /bin/sh
# Copyright (C) 2021 Facebook
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

# // Program processed by https://github.com/facebookincubator/BOLT.
# // gcc -g -O2 -fno-reorder-blocks-and-partition -Wl,--emit-relocs primes.c -o primes
# // perf record -e cycles:u -j any,u -o perf.data -- ./primes 1000 > /dev/null
# // perf2bolt -p perf.data -o perf.fdata ./primes
# // llvm-bolt primes -o primes.bolt -data=perf.fdata -reorder-blocks=cache+ -reorder-functions=hfsort -split-functions=2 -split-all-cold -split-eh -dyno-stats -update-debug-sections
#
# #include <inttypes.h>
# #include <stdbool.h>
# #include <stdio.h>
# #include <stdlib.h>
# 
# bool
# is_prime (uint32_t n)
# {
#   if (n < 2)
#     return false;
#   if (n == 2)
#     return true;
#   if (n % 2 == 0)
#     return false;
#   for (uint32_t i = 3; i <= n / 2; i++)
#     {
#       if (n % i == 0)
# 	return false;
#     }
#   return true;
# }
# 
# int
# main (int argc, char *argv[])
# {
#   if (argc != 2)
#     return EXIT_FAILURE;
#   int n = atoi (argv[1]);
#   for (uint32_t i = 2; n > 0; i++)
#     {
#       if (is_prime (i))
# 	{
# 	  printf ("%" PRIu32 "\n", i);
# 	  n--;
# 	}
#     }
#   return EXIT_SUCCESS;
# }
testfiles testfile-dw-form-indirect

testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=info testfile-dw-form-indirect << EOF

DWARF section [33] '.debug_info' at offset 0x801db0:
 [Offset]
 Compilation unit at offset 0:
 Version: 4, Abbreviation section offset: 0, Address size: 8, Offset size: 4
 [     b]  compile_unit         abbrev: 1
           producer             (strp) "GNU C17 8.4.1 20200928 (Red Hat 8.4.1-1) -mtune=generic -march=x86-64 -g -O2 -fno-reorder-blocks-and-partition"
           language             (data1) C99 (12)
           name                 (strp) "primes.c"
           comp_dir             (strp) "/home/osandov/bolt"
           ranges               (sec_offset) range list [    10]
           low_pc               (addr) 000000000000000000
           stmt_list            (sec_offset) 0
 [    29]    base_type            abbrev: 2
             byte_size            (data1) 1
             encoding             (data1) unsigned_char (8)
             name                 (strp) "unsigned char"
 [    30]    base_type            abbrev: 2
             byte_size            (data1) 2
             encoding             (data1) unsigned (7)
             name                 (strp) "short unsigned int"
 [    37]    base_type            abbrev: 2
             byte_size            (data1) 4
             encoding             (data1) unsigned (7)
             name                 (strp) "unsigned int"
 [    3e]    base_type            abbrev: 2
             byte_size            (data1) 8
             encoding             (data1) unsigned (7)
             name                 (strp) "long unsigned int"
 [    45]    base_type            abbrev: 2
             byte_size            (data1) 1
             encoding             (data1) signed_char (6)
             name                 (strp) "signed char"
 [    4c]    base_type            abbrev: 2
             byte_size            (data1) 2
             encoding             (data1) signed (5)
             name                 (strp) "short int"
 [    53]    base_type            abbrev: 3
             byte_size            (data1) 4
             encoding             (data1) signed (5)
             name                 (string) "int"
 [    5a]    typedef              abbrev: 4
             name                 (strp) "__uint32_t"
             decl_file            (data1) types.h (3)
             decl_line            (data1) 41
             decl_column          (data1) 22
             type                 (ref4) [    37]
 [    66]    base_type            abbrev: 2
             byte_size            (data1) 8
             encoding             (data1) signed (5)
             name                 (strp) "long int"
 [    6d]    typedef              abbrev: 4
             name                 (strp) "__off_t"
             decl_file            (data1) types.h (3)
             decl_line            (data1) 150
             decl_column          (data1) 25
             type                 (ref4) [    66]
 [    79]    typedef              abbrev: 4
             name                 (strp) "__off64_t"
             decl_file            (data1) types.h (3)
             decl_line            (data1) 151
             decl_column          (data1) 27
             type                 (ref4) [    66]
 [    85]    pointer_type         abbrev: 5
             byte_size            (data1) 8
 [    87]    pointer_type         abbrev: 6
             byte_size            (data1) 8
             type                 (ref4) [    8d]
 [    8d]    base_type            abbrev: 2
             byte_size            (data1) 1
             encoding             (data1) signed_char (6)
             name                 (strp) "char"
 [    94]    const_type           abbrev: 7
             type                 (ref4) [    8d]
 [    99]    typedef              abbrev: 4
             name                 (strp) "uint32_t"
             decl_file            (data1) stdint-uintn.h (4)
             decl_line            (data1) 26
             decl_column          (data1) 20
             type                 (ref4) [    5a]
 [    a5]    typedef              abbrev: 4
             name                 (strp) "size_t"
             decl_file            (data1) stddef.h (5)
             decl_line            (data1) 216
             decl_column          (data1) 23
             type                 (ref4) [    3e]
 [    b1]    structure_type       abbrev: 8
             name                 (strp) "_IO_FILE"
             byte_size            (data1) 216
             decl_file            (data1) struct_FILE.h (6)
             decl_line            (data1) 49
             decl_column          (data1) 8
             sibling              (ref4) [   238]
 [    be]      member               abbrev: 9
               name                 (strp) "_flags"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 51
               decl_column          (data1) 7
               type                 (ref4) [    53]
               data_member_location (data1) 0
 [    cb]      member               abbrev: 9
               name                 (strp) "_IO_read_ptr"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 54
               decl_column          (data1) 9
               type                 (ref4) [    87]
               data_member_location (data1) 8
 [    d8]      member               abbrev: 9
               name                 (strp) "_IO_read_end"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 55
               decl_column          (data1) 9
               type                 (ref4) [    87]
               data_member_location (data1) 16
 [    e5]      member               abbrev: 9
               name                 (strp) "_IO_read_base"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 56
               decl_column          (data1) 9
               type                 (ref4) [    87]
               data_member_location (data1) 24
 [    f2]      member               abbrev: 9
               name                 (strp) "_IO_write_base"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 57
               decl_column          (data1) 9
               type                 (ref4) [    87]
               data_member_location (data1) 32
 [    ff]      member               abbrev: 9
               name                 (strp) "_IO_write_ptr"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 58
               decl_column          (data1) 9
               type                 (ref4) [    87]
               data_member_location (data1) 40
 [   10c]      member               abbrev: 9
               name                 (strp) "_IO_write_end"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 59
               decl_column          (data1) 9
               type                 (ref4) [    87]
               data_member_location (data1) 48
 [   119]      member               abbrev: 9
               name                 (strp) "_IO_buf_base"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 60
               decl_column          (data1) 9
               type                 (ref4) [    87]
               data_member_location (data1) 56
 [   126]      member               abbrev: 9
               name                 (strp) "_IO_buf_end"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 61
               decl_column          (data1) 9
               type                 (ref4) [    87]
               data_member_location (data1) 64
 [   133]      member               abbrev: 9
               name                 (strp) "_IO_save_base"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 64
               decl_column          (data1) 9
               type                 (ref4) [    87]
               data_member_location (data1) 72
 [   140]      member               abbrev: 9
               name                 (strp) "_IO_backup_base"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 65
               decl_column          (data1) 9
               type                 (ref4) [    87]
               data_member_location (data1) 80
 [   14d]      member               abbrev: 9
               name                 (strp) "_IO_save_end"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 66
               decl_column          (data1) 9
               type                 (ref4) [    87]
               data_member_location (data1) 88
 [   15a]      member               abbrev: 9
               name                 (strp) "_markers"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 68
               decl_column          (data1) 22
               type                 (ref4) [   251]
               data_member_location (data1) 96
 [   167]      member               abbrev: 9
               name                 (strp) "_chain"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 70
               decl_column          (data1) 20
               type                 (ref4) [   257]
               data_member_location (data1) 104
 [   174]      member               abbrev: 9
               name                 (strp) "_fileno"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 72
               decl_column          (data1) 7
               type                 (ref4) [    53]
               data_member_location (data1) 112
 [   181]      member               abbrev: 9
               name                 (strp) "_flags2"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 73
               decl_column          (data1) 7
               type                 (ref4) [    53]
               data_member_location (data1) 116
 [   18e]      member               abbrev: 9
               name                 (strp) "_old_offset"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 74
               decl_column          (data1) 11
               type                 (ref4) [    6d]
               data_member_location (data1) 120
 [   19b]      member               abbrev: 9
               name                 (strp) "_cur_column"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 77
               decl_column          (data1) 18
               type                 (ref4) [    30]
               data_member_location (data1) 128
 [   1a8]      member               abbrev: 9
               name                 (strp) "_vtable_offset"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 78
               decl_column          (data1) 15
               type                 (ref4) [    45]
               data_member_location (data1) 130
 [   1b5]      member               abbrev: 9
               name                 (strp) "_shortbuf"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 79
               decl_column          (data1) 8
               type                 (ref4) [   25d]
               data_member_location (data1) 131
 [   1c2]      member               abbrev: 9
               name                 (strp) "_lock"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 81
               decl_column          (data1) 15
               type                 (ref4) [   26d]
               data_member_location (data1) 136
 [   1cf]      member               abbrev: 9
               name                 (strp) "_offset"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 89
               decl_column          (data1) 13
               type                 (ref4) [    79]
               data_member_location (data1) 144
 [   1dc]      member               abbrev: 9
               name                 (strp) "_codecvt"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 91
               decl_column          (data1) 23
               type                 (ref4) [   278]
               data_member_location (data1) 152
 [   1e9]      member               abbrev: 9
               name                 (strp) "_wide_data"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 92
               decl_column          (data1) 25
               type                 (ref4) [   283]
               data_member_location (data1) 160
 [   1f6]      member               abbrev: 9
               name                 (strp) "_freeres_list"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 93
               decl_column          (data1) 20
               type                 (ref4) [   257]
               data_member_location (data1) 168
 [   203]      member               abbrev: 9
               name                 (strp) "_freeres_buf"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 94
               decl_column          (data1) 9
               type                 (ref4) [    85]
               data_member_location (data1) 176
 [   210]      member               abbrev: 9
               name                 (strp) "__pad5"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 95
               decl_column          (data1) 10
               type                 (ref4) [    a5]
               data_member_location (data1) 184
 [   21d]      member               abbrev: 9
               name                 (strp) "_mode"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 96
               decl_column          (data1) 7
               type                 (ref4) [    53]
               data_member_location (data1) 192
 [   22a]      member               abbrev: 9
               name                 (strp) "_unused2"
               decl_file            (data1) struct_FILE.h (6)
               decl_line            (data1) 98
               decl_column          (data1) 8
               type                 (ref4) [   289]
               data_member_location (data1) 196
 [   238]    typedef              abbrev: 4
             name                 (strp) "FILE"
             decl_file            (data1) FILE.h (7)
             decl_line            (data1) 7
             decl_column          (data1) 25
             type                 (ref4) [    b1]
 [   244]    typedef              abbrev: 10
             name                 (strp) "_IO_lock_t"
             decl_file            (data1) struct_FILE.h (6)
             decl_line            (data1) 43
             decl_column          (data1) 14
 [   24c]    structure_type       abbrev: 11
             name                 (strp) "_IO_marker"
             declaration          (flag_present) yes
 [   251]    pointer_type         abbrev: 6
             byte_size            (data1) 8
             type                 (ref4) [   24c]
 [   257]    pointer_type         abbrev: 6
             byte_size            (data1) 8
             type                 (ref4) [    b1]
 [   25d]    array_type           abbrev: 12
             type                 (ref4) [    8d]
             sibling              (ref4) [   26d]
 [   266]      subrange_type        abbrev: 13
               type                 (ref4) [    3e]
               upper_bound          (data1) 0
 [   26d]    pointer_type         abbrev: 6
             byte_size            (data1) 8
             type                 (ref4) [   244]
 [   273]    structure_type       abbrev: 11
             name                 (strp) "_IO_codecvt"
             declaration          (flag_present) yes
 [   278]    pointer_type         abbrev: 6
             byte_size            (data1) 8
             type                 (ref4) [   273]
 [   27e]    structure_type       abbrev: 11
             name                 (strp) "_IO_wide_data"
             declaration          (flag_present) yes
 [   283]    pointer_type         abbrev: 6
             byte_size            (data1) 8
             type                 (ref4) [   27e]
 [   289]    array_type           abbrev: 12
             type                 (ref4) [    8d]
             sibling              (ref4) [   299]
 [   292]      subrange_type        abbrev: 13
               type                 (ref4) [    3e]
               upper_bound          (data1) 19
 [   299]    variable             abbrev: 14
             name                 (strp) "stdin"
             decl_file            (data1) stdio.h (8)
             decl_line            (data1) 137
             decl_column          (data1) 14
             type                 (ref4) [   2a5]
             external             (flag_present) yes
             declaration          (flag_present) yes
 [   2a5]    pointer_type         abbrev: 6
             byte_size            (data1) 8
             type                 (ref4) [   238]
 [   2ab]    variable             abbrev: 14
             name                 (strp) "stdout"
             decl_file            (data1) stdio.h (8)
             decl_line            (data1) 138
             decl_column          (data1) 14
             type                 (ref4) [   2a5]
             external             (flag_present) yes
             declaration          (flag_present) yes
 [   2b7]    variable             abbrev: 14
             name                 (strp) "stderr"
             decl_file            (data1) stdio.h (8)
             decl_line            (data1) 139
             decl_column          (data1) 14
             type                 (ref4) [   2a5]
             external             (flag_present) yes
             declaration          (flag_present) yes
 [   2c3]    variable             abbrev: 14
             name                 (strp) "sys_nerr"
             decl_file            (data1) sys_errlist.h (9)
             decl_line            (data1) 26
             decl_column          (data1) 12
             type                 (ref4) [    53]
             external             (flag_present) yes
             declaration          (flag_present) yes
 [   2cf]    array_type           abbrev: 12
             type                 (ref4) [   2e5]
             sibling              (ref4) [   2da]
 [   2d8]      subrange_type        abbrev: 15
 [   2da]    const_type           abbrev: 7
             type                 (ref4) [   2cf]
 [   2df]    pointer_type         abbrev: 6
             byte_size            (data1) 8
             type                 (ref4) [    94]
 [   2e5]    const_type           abbrev: 7
             type                 (ref4) [   2df]
 [   2ea]    variable             abbrev: 14
             name                 (strp) "sys_errlist"
             decl_file            (data1) sys_errlist.h (9)
             decl_line            (data1) 27
             decl_column          (data1) 26
             type                 (ref4) [   2da]
             external             (flag_present) yes
             declaration          (flag_present) yes
 [   2f6]    base_type            abbrev: 2
             byte_size            (data1) 8
             encoding             (data1) signed (5)
             name                 (strp) "long long int"
 [   2fd]    base_type            abbrev: 2
             byte_size            (data1) 8
             encoding             (data1) unsigned (7)
             name                 (strp) "long long unsigned int"
 [   304]    subprogram           abbrev: 16
             external             (flag_present) yes
             name                 (strp) "main"
             decl_file            (data1) primes.c (1)
             decl_line            (data1) 24
             decl_column          (data1) 1
             prototyped           (flag_present) yes
             type                 (ref4) [    53]
             ranges               (sec_offset) range list [    50]
             low_pc               (addr) 000000000000000000
             frame_base           (exprloc) 
              [ 0] call_frame_cfa
             GNU_all_call_sites   (flag_present) yes
             sibling              (ref4) [   429]
 [   326]      formal_parameter     abbrev: 17
               name                 (strp) "argc"
               decl_file            (data1) primes.c (1)
               decl_line            (data1) 24
               decl_column          (data1) 11
               type                 (ref4) [    53]
               location             (sec_offset) location list [    10]
               GNU_locviews         (sec_offset) location list [     0]
 [   33a]      formal_parameter     abbrev: 17
               name                 (strp) "argv"
               decl_file            (data1) primes.c (1)
               decl_line            (data1) 24
               decl_column          (data1) 23
               type                 (ref4) [   429]
               location             (sec_offset) location list [    72]
               GNU_locviews         (sec_offset) location list [    3d]
 [   34e]      variable             abbrev: 18
               name                 (string) "n"
               decl_file            (data1) primes.c (1)
               decl_line            (data1) 28
               decl_column          (data1) 7
               type                 (ref4) [    53]
               location             (sec_offset) location list [    d4]
               GNU_locviews         (sec_offset) location list [    7a]
 [   360]      lexical_block        abbrev: 19
               ranges               (sec_offset) range list [    80]
               sibling              (ref4) [   3ed]
 [   369]        variable             abbrev: 18
                 name                 (string) "i"
                 decl_file            (data1) primes.c (1)
                 decl_line            (data1) 29
                 decl_column          (data1) 17
                 type                 (ref4) [    99]
                 location             (sec_offset) location list [   158]
                 GNU_locviews         (sec_offset) location list [    f5]
 [   37b]        inlined_subroutine   abbrev: 20
                 abstract_origin      (ref4) [   42f]
                 entry_pc             (addr) 0x0000000000400520
                 GNU_entry_view       (data1) 6
                 ranges               (sec_offset) range list [    e0]
                 call_file            (data1) primes.c (1)
                 call_line            (data1) 31
                 call_column          (data1) 11
                 sibling              (ref4) [   3cb]
 [   394]          formal_parameter     abbrev: 21
                   abstract_origin      (ref4) [   440]
                   location             (sec_offset) location list [   1dd]
                   GNU_locviews         (sec_offset) location list [   186]
 [   3a1]          inlined_subroutine   abbrev: 22
                   abstract_origin      (ref4) [   42f]
                   ranges               (sec_offset) range list [   120]
                   call_file            (data1) primes.c (1)
                   call_line            (data1) 7
                   call_column          (data1) 1
 [   3ad]            formal_parameter     abbrev: 23
                     abstract_origin      (ref4) [   440]
 [   3b2]            lexical_block        abbrev: 24
                     abstract_origin      (ref4) [   44a]
                     ranges               (sec_offset) range list [   120]
 [   3bb]              variable             abbrev: 25
                       abstract_origin      (ref4) [   44b]
                       location             (sec_offset) location list [   250]
                       GNU_locviews         (sec_offset) location list [   203]
 [   3cb]        GNU_call_site        abbrev: 26
                 low_pc               (addr) 0x0000000000a000c7 <.annobin_init.c.unlikely.cold.0+0x47>
                 abstract_origin      (ref4) [   4e2]
 [   3d8]          GNU_call_site_parameter abbrev: 27
                   location             (exprloc) 
                    [ 0] reg5
                   GNU_call_site_value  (exprloc) 
                    [ 0] addr 0x400788 <__dso_handle+0x8>
 [   3e5]          GNU_call_site_parameter abbrev: 27
                   location             (exprloc) 
                    [ 0] reg4
                   GNU_call_site_value  (exprloc) 
                    [ 0] breg3 -1
 [   3ed]      inlined_subroutine   abbrev: 28
               abstract_origin      (ref4) [   45e]
               entry_pc             (addr) 0x00000000004004fb
               GNU_entry_view       (data1) 1
               ranges               (sec_offset) range list [   150]
               call_file            (data1) primes.c (1)
               call_line            (data1) 28
               call_column          (data1) 11
 [   402]        formal_parameter     abbrev: 21
                 abstract_origin      (ref4) [   470]
                 location             (sec_offset) location list [   2ad]
                 GNU_locviews         (sec_offset) location list [   268]
 [   40f]        GNU_call_site        abbrev: 26
                 low_pc               (addr) 0x0000000000a000a4 <.annobin_init.c.unlikely.cold.0+0x24>
                 abstract_origin      (ref4) [   4ef]
 [   41c]          GNU_call_site_parameter abbrev: 27
                   location             (exprloc) 
                    [ 0] reg4
                   GNU_call_site_value  (exprloc) 
                    [ 0] lit0
 [   421]          GNU_call_site_parameter abbrev: 27
                   location             (exprloc) 
                    [ 0] reg1
                   GNU_call_site_value  (exprloc) 
                    [ 0] lit10
 [   429]    pointer_type         abbrev: 6
             byte_size            (data1) 8
             type                 (ref4) [    87]
 [   42f]    subprogram           abbrev: 29
             external             (flag_present) yes
             name                 (strp) "is_prime"
             decl_file            (data1) primes.c (1)
             decl_line            (data1) 7
             decl_column          (data1) 1
             prototyped           (flag_present) yes
             type                 (ref4) [   457]
             inline               (data1) inlined (1)
             sibling              (ref4) [   457]
 [   440]      formal_parameter     abbrev: 30
               name                 (string) "n"
               decl_file            (data1) primes.c (1)
               decl_line            (data1) 7
               decl_column          (data1) 20
               type                 (ref4) [    99]
 [   44a]      lexical_block        abbrev: 31
 [   44b]        variable             abbrev: 32
                 name                 (string) "i"
                 decl_file            (data1) primes.c (1)
                 decl_line            (data1) 15
                 decl_column          (data1) 17
                 type                 (ref4) [    99]
 [   457]    base_type            abbrev: 2
             byte_size            (data1) 1
             encoding             (data1) boolean (2)
             name                 (strp) "_Bool"
 [   45e]    subprogram           abbrev: 33
             external             (flag_present) yes
             name                 (strp) "atoi"
             decl_file            (data1) stdlib.h (2)
             decl_line            (data2) 361
             decl_column          (data1) 1
             prototyped           (flag_present) yes
             type                 (ref4) [    53]
             inline               (data1) declared_inlined (3)
             sibling              (ref4) [   47e]
 [   470]      formal_parameter     abbrev: 34
               name                 (strp) "__nptr"
               decl_file            (data1) stdlib.h (2)
               decl_line            (data2) 361
               decl_column          (data1) 1
               type                 (ref4) [   2df]
 [   47e]    subprogram           abbrev: 35
             abstract_origin      (ref4) [   42f]
             low_pc               (addr) 0x0000000000400680 <is_prime>
             high_pc              (data8) 101 (0x00000000004006e5)
             frame_base           (exprloc) 
              [ 0] call_frame_cfa
             GNU_all_call_sites   (flag_present) yes
             sibling              (ref4) [   4e2]
 [   499]      formal_parameter     abbrev: 36
               abstract_origin      (ref4) [   440]
               location             (exprloc) 
                [ 0] reg5
 [   4a0]      inlined_subroutine   abbrev: 37
               abstract_origin      (ref4) [   42f]
               ranges               (sec_offset) range list [   1a0]
               low_pc               (addr) 000000000000000000
               call_file            (data1) primes.c (1)
               call_line            (data1) 7
               call_column          (data1) 1
 [   4b8]        formal_parameter     abbrev: 23
                 abstract_origin      (ref4) [   440]
 [   4bd]        lexical_block        abbrev: 38
                 abstract_origin      (ref4) [   44a]
                 ranges               (sec_offset) range list [   1a0]
                 low_pc               (addr) 000000000000000000
 [   4d2]          variable             abbrev: 25
                   abstract_origin      (ref4) [   44b]
                   location             (sec_offset) location list [   2d1]
                   GNU_locviews         (sec_offset) location list [   28e]
 [   4e2]    subprogram           abbrev: 39
             external             (flag_present) yes
             declaration          (flag_present) yes
             linkage_name         (strp) "printf"
             name                 (strp) "printf"
             decl_file            (data1) stdio.h (8)
             decl_line            (data2) 332
             decl_column          (data1) 12
 [   4ef]    subprogram           abbrev: 40
             external             (flag_present) yes
             declaration          (flag_present) yes
             linkage_name         (strp) "strtol"
             name                 (strp) "strtol"
             decl_file            (data1) stdlib.h (2)
             decl_line            (data1) 176
             decl_column          (data1) 17
EOF
