#! /bin/sh
# Copyright (C) 1999, 2000, 2002, 2003, 2004, 2005 Red Hat, Inc.
# This file is part of elfutils.
# Written by Ulrich Drepper <drepper@redhat.com>, 1999.
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

testfiles testfile5 testfile2

testrun_compare ${abs_builddir}/show-die-info testfile5 testfile2 <<\EOF
file: testfile5
New CU: off = 0, hsize = 11, ab = 0, as = 4, os = 4
     DW_TAG_compile_unit
      Name      : b.c
      Offset    : 11
      CU offset : 11
      Attrs     : name stmt_list low_pc high_pc language comp_dir producer
      low PC    : 0x804842c
      high PC   : 0x8048436
      language  : 1
      directory : /home/drepper/gnu/new-bu/build/ttt
      producer  : GNU C 2.96 20000731 (Red Hat Linux 7.0)
          DW_TAG_subprogram
           Name      : bar
           Offset    : 104
           CU offset : 104
           Attrs     : name low_pc high_pc prototyped decl_file decl_line external frame_base type
           low PC    : 0x804842c
           high PC   : 0x8048436
          DW_TAG_base_type
           Name      : int
           Offset    : 127
           CU offset : 127
           Attrs     : name byte_size encoding
           byte size : 4
New CU: off = 135, hsize = 11, ab = 54, as = 4, os = 4
     DW_TAG_compile_unit
      Name      : f.c
      Offset    : 146
      CU offset : 11
      Attrs     : name stmt_list low_pc high_pc language comp_dir producer
      low PC    : 0x8048438
      high PC   : 0x8048442
      language  : 1
      directory : /home/drepper/gnu/new-bu/build/ttt
      producer  : GNU C 2.96 20000731 (Red Hat Linux 7.0)
          DW_TAG_subprogram
           Name      : foo
           Offset    : 239
           CU offset : 104
           Attrs     : name low_pc high_pc prototyped decl_file decl_line external frame_base type
           low PC    : 0x8048438
           high PC   : 0x8048442
          DW_TAG_base_type
           Name      : int
           Offset    : 262
           CU offset : 127
           Attrs     : name byte_size encoding
           byte size : 4
New CU: off = 270, hsize = 11, ab = 108, as = 4, os = 4
     DW_TAG_compile_unit
      Name      : m.c
      Offset    : 281
      CU offset : 11
      Attrs     : name stmt_list low_pc high_pc language comp_dir producer
      low PC    : 0x8048444
      high PC   : 0x8048472
      language  : 1
      directory : /home/drepper/gnu/new-bu/build/ttt
      producer  : GNU C 2.96 20000731 (Red Hat Linux 7.0)
          DW_TAG_subprogram
           Name      : main
           Offset    : 374
           CU offset : 104
           Attrs     : sibling name low_pc high_pc prototyped decl_file decl_line external frame_base type
           low PC    : 0x8048444
           high PC   : 0x8048472
               DW_TAG_subprogram
                Name      : bar
                Offset    : 402
                CU offset : 132
                Attrs     : sibling name decl_file decl_line declaration external type
                    DW_TAG_unspecified_parameters
                     Name      : * NO NAME *
                     Offset    : 419
                     CU offset : 149
                     Attrs     :
               DW_TAG_subprogram
                Name      : foo
                Offset    : 421
                CU offset : 151
                Attrs     : name decl_file decl_line declaration external type
                    DW_TAG_unspecified_parameters
                     Name      : * NO NAME *
                     Offset    : 434
                     CU offset : 164
                     Attrs     :
          DW_TAG_base_type
           Name      : int
           Offset    : 437
           CU offset : 167
           Attrs     : name byte_size encoding
           byte size : 4
          DW_TAG_variable
           Name      : a
           Offset    : 444
           CU offset : 174
           Attrs     : location name decl_file decl_line external type
file: testfile2
New CU: off = 0, hsize = 11, ab = 0, as = 4, os = 4
     DW_TAG_compile_unit
      Name      : b.c
      Offset    : 11
      CU offset : 11
      Attrs     : name stmt_list low_pc high_pc language comp_dir producer
      low PC    : 0x10000470
      high PC   : 0x10000490
      language  : 1
      directory : /shoggoth/drepper
      producer  : GNU C 2.96-laurel-000912
          DW_TAG_subprogram
           Name      : bar
           Offset    : 72
           CU offset : 72
           Attrs     : name low_pc high_pc prototyped decl_file decl_line external frame_base type
           low PC    : 0x10000470
           high PC   : 0x10000490
          DW_TAG_base_type
           Name      : int
           Offset    : 95
           CU offset : 95
           Attrs     : name byte_size encoding
           byte size : 4
          DW_TAG_typedef
           Name      : size_t
           Offset    : 102
           CU offset : 102
           Attrs     : name decl_file decl_line type
          DW_TAG_base_type
           Name      : unsigned int
           Offset    : 116
           CU offset : 116
           Attrs     : name byte_size encoding
           byte size : 4
          DW_TAG_typedef
           Name      : __gnuc_va_list
           Offset    : 132
           CU offset : 132
           Attrs     : name decl_file decl_line type
          DW_TAG_array_type
           Name      : __builtin_va_list
           Offset    : 154
           CU offset : 154
           Attrs     : sibling name type
               DW_TAG_subrange_type
                Name      : * NO NAME *
                Offset    : 181
                CU offset : 181
                Attrs     : upper_bound type
          DW_TAG_base_type
           Name      : unsigned int
           Offset    : 188
           CU offset : 188
           Attrs     : name byte_size encoding
           byte size : 4
          DW_TAG_structure_type
           Name      : __va_list_tag
           Offset    : 204
           CU offset : 204
           Attrs     : sibling name byte_size decl_file decl_line
           byte size : 12
               DW_TAG_member
                Name      : gpr
                Offset    : 226
                CU offset : 226
                Attrs     : name data_member_location decl_file decl_line type
               DW_TAG_member
                Name      : fpr
                Offset    : 240
                CU offset : 240
                Attrs     : name data_member_location decl_file decl_line type
               DW_TAG_member
                Name      : overflow_arg_area
                Offset    : 254
                CU offset : 254
                Attrs     : name data_member_location decl_file decl_line type
               DW_TAG_member
                Name      : reg_save_area
                Offset    : 282
                CU offset : 282
                Attrs     : name data_member_location decl_file decl_line type
          DW_TAG_base_type
           Name      : unsigned char
           Offset    : 307
           CU offset : 307
           Attrs     : name byte_size encoding
           byte size : 1
          DW_TAG_pointer_type
           Name      : * NO NAME *
           Offset    : 324
           CU offset : 324
           Attrs     : byte_size
           byte size : 4
          DW_TAG_typedef
           Name      : __u_char
           Offset    : 326
           CU offset : 326
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __u_short
           Offset    : 342
           CU offset : 342
           Attrs     : name decl_file decl_line type
          DW_TAG_base_type
           Name      : short unsigned int
           Offset    : 359
           CU offset : 359
           Attrs     : name byte_size encoding
           byte size : 2
          DW_TAG_typedef
           Name      : __u_int
           Offset    : 381
           CU offset : 381
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __u_long
           Offset    : 396
           CU offset : 396
           Attrs     : name decl_file decl_line type
          DW_TAG_base_type
           Name      : long unsigned int
           Offset    : 412
           CU offset : 412
           Attrs     : name byte_size encoding
           byte size : 4
          DW_TAG_typedef
           Name      : __u_quad_t
           Offset    : 433
           CU offset : 433
           Attrs     : name decl_file decl_line type
          DW_TAG_base_type
           Name      : long long unsigned int
           Offset    : 451
           CU offset : 451
           Attrs     : name byte_size encoding
           byte size : 8
          DW_TAG_typedef
           Name      : __quad_t
           Offset    : 477
           CU offset : 477
           Attrs     : name decl_file decl_line type
          DW_TAG_base_type
           Name      : long long int
           Offset    : 493
           CU offset : 493
           Attrs     : name byte_size encoding
           byte size : 8
          DW_TAG_typedef
           Name      : __int8_t
           Offset    : 510
           CU offset : 510
           Attrs     : name decl_file decl_line type
          DW_TAG_base_type
           Name      : signed char
           Offset    : 526
           CU offset : 526
           Attrs     : name byte_size encoding
           byte size : 1
          DW_TAG_typedef
           Name      : __uint8_t
           Offset    : 541
           CU offset : 541
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __int16_t
           Offset    : 558
           CU offset : 558
           Attrs     : name decl_file decl_line type
          DW_TAG_base_type
           Name      : short int
           Offset    : 575
           CU offset : 575
           Attrs     : name byte_size encoding
           byte size : 2
          DW_TAG_typedef
           Name      : __uint16_t
           Offset    : 588
           CU offset : 588
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __int32_t
           Offset    : 606
           CU offset : 606
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __uint32_t
           Offset    : 623
           CU offset : 623
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __int64_t
           Offset    : 641
           CU offset : 641
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __uint64_t
           Offset    : 658
           CU offset : 658
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __qaddr_t
           Offset    : 676
           CU offset : 676
           Attrs     : name decl_file decl_line type
          DW_TAG_pointer_type
           Name      : * NO NAME *
           Offset    : 693
           CU offset : 693
           Attrs     : byte_size type
           byte size : 4
          DW_TAG_typedef
           Name      : __dev_t
           Offset    : 699
           CU offset : 699
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __uid_t
           Offset    : 714
           CU offset : 714
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __gid_t
           Offset    : 729
           CU offset : 729
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __ino_t
           Offset    : 744
           CU offset : 744
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __mode_t
           Offset    : 759
           CU offset : 759
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __nlink_t
           Offset    : 775
           CU offset : 775
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __off_t
           Offset    : 792
           CU offset : 792
           Attrs     : name decl_file decl_line type
          DW_TAG_base_type
           Name      : long int
           Offset    : 807
           CU offset : 807
           Attrs     : name byte_size encoding
           byte size : 4
          DW_TAG_typedef
           Name      : __loff_t
           Offset    : 819
           CU offset : 819
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __pid_t
           Offset    : 835
           CU offset : 835
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __ssize_t
           Offset    : 850
           CU offset : 850
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __rlim_t
           Offset    : 867
           CU offset : 867
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __rlim64_t
           Offset    : 883
           CU offset : 883
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __id_t
           Offset    : 901
           CU offset : 901
           Attrs     : name decl_file decl_line type
          DW_TAG_structure_type
           Name      : * NO NAME *
           Offset    : 915
           CU offset : 915
           Attrs     : sibling byte_size decl_file decl_line
           byte size : 8
               DW_TAG_member
                Name      : __val
                Offset    : 923
                CU offset : 923
                Attrs     : name data_member_location decl_file decl_line type
          DW_TAG_array_type
           Name      : * NO NAME *
           Offset    : 940
           CU offset : 940
           Attrs     : sibling type
               DW_TAG_subrange_type
                Name      : * NO NAME *
                Offset    : 949
                CU offset : 949
                Attrs     : upper_bound type
          DW_TAG_typedef
           Name      : __fsid_t
           Offset    : 956
           CU offset : 956
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __daddr_t
           Offset    : 972
           CU offset : 972
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __caddr_t
           Offset    : 989
           CU offset : 989
           Attrs     : name decl_file decl_line type
          DW_TAG_pointer_type
           Name      : * NO NAME *
           Offset    : 1006
           CU offset : 1006
           Attrs     : byte_size type
           byte size : 4
          DW_TAG_base_type
           Name      : char
           Offset    : 1012
           CU offset : 1012
           Attrs     : name byte_size encoding
           byte size : 1
          DW_TAG_typedef
           Name      : __time_t
           Offset    : 1020
           CU offset : 1020
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __swblk_t
           Offset    : 1036
           CU offset : 1036
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __clock_t
           Offset    : 1053
           CU offset : 1053
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __fd_mask
           Offset    : 1070
           CU offset : 1070
           Attrs     : name decl_file decl_line type
          DW_TAG_structure_type
           Name      : * NO NAME *
           Offset    : 1087
           CU offset : 1087
           Attrs     : sibling byte_size decl_file decl_line
           byte size : 128
               DW_TAG_member
                Name      : __fds_bits
                Offset    : 1095
                CU offset : 1095
                Attrs     : name data_member_location decl_file decl_line type
          DW_TAG_array_type
           Name      : * NO NAME *
           Offset    : 1117
           CU offset : 1117
           Attrs     : sibling type
               DW_TAG_subrange_type
                Name      : * NO NAME *
                Offset    : 1126
                CU offset : 1126
                Attrs     : upper_bound type
          DW_TAG_typedef
           Name      : __fd_set
           Offset    : 1133
           CU offset : 1133
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __key_t
           Offset    : 1149
           CU offset : 1149
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __ipc_pid_t
           Offset    : 1164
           CU offset : 1164
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __blkcnt_t
           Offset    : 1183
           CU offset : 1183
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __blkcnt64_t
           Offset    : 1201
           CU offset : 1201
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __fsblkcnt_t
           Offset    : 1221
           CU offset : 1221
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __fsblkcnt64_t
           Offset    : 1241
           CU offset : 1241
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __fsfilcnt_t
           Offset    : 1263
           CU offset : 1263
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __fsfilcnt64_t
           Offset    : 1283
           CU offset : 1283
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __ino64_t
           Offset    : 1305
           CU offset : 1305
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __off64_t
           Offset    : 1322
           CU offset : 1322
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __t_scalar_t
           Offset    : 1339
           CU offset : 1339
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __t_uscalar_t
           Offset    : 1359
           CU offset : 1359
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : __intptr_t
           Offset    : 1380
           CU offset : 1380
           Attrs     : name decl_file decl_line type
          DW_TAG_structure_type
           Name      : _IO_FILE
           Offset    : 1398
           CU offset : 1398
           Attrs     : sibling name byte_size decl_file decl_line
           byte size : 152
               DW_TAG_member
                Name      : _flags
                Offset    : 1415
                CU offset : 1415
                Attrs     : name data_member_location decl_file decl_line type
               DW_TAG_member
                Name      : _IO_read_ptr
                Offset    : 1432
                CU offset : 1432
                Attrs     : name data_member_location decl_file decl_line type
               DW_TAG_member
                Name      : _IO_read_end
                Offset    : 1455
                CU offset : 1455
                Attrs     : name data_member_location decl_file decl_line type
               DW_TAG_member
                Name      : _IO_read_base
                Offset    : 1478
                CU offset : 1478
                Attrs     : name data_member_location decl_file decl_line type
               DW_TAG_member
                Name      : _IO_write_base
                Offset    : 1502
                CU offset : 1502
                Attrs     : name data_member_location decl_file decl_line type
               DW_TAG_member
                Name      : _IO_write_ptr
                Offset    : 1527
                CU offset : 1527
                Attrs     : name data_member_location decl_file decl_line type
               DW_TAG_member
                Name      : _IO_write_end
                Offset    : 1551
                CU offset : 1551
                Attrs     : name data_member_location decl_file decl_line type
               DW_TAG_member
                Name      : _IO_buf_base
                Offset    : 1575
                CU offset : 1575
                Attrs     : name data_member_location decl_file decl_line type
               DW_TAG_member
                Name      : _IO_buf_end
                Offset    : 1598
                CU offset : 1598
                Attrs     : name data_member_location decl_file decl_line type
               DW_TAG_member
                Name      : _IO_save_base
                Offset    : 1620
                CU offset : 1620
                Attrs     : name data_member_location decl_file decl_line type
               DW_TAG_member
                Name      : _IO_backup_base
                Offset    : 1644
                CU offset : 1644
                Attrs     : name data_member_location decl_file decl_line type
               DW_TAG_member
                Name      : _IO_save_end
                Offset    : 1670
                CU offset : 1670
                Attrs     : name data_member_location decl_file decl_line type
               DW_TAG_member
                Name      : _markers
                Offset    : 1693
                CU offset : 1693
                Attrs     : name data_member_location decl_file decl_line type
               DW_TAG_member
                Name      : _chain
                Offset    : 1712
                CU offset : 1712
                Attrs     : name data_member_location decl_file decl_line type
               DW_TAG_member
                Name      : _fileno
                Offset    : 1729
                CU offset : 1729
                Attrs     : name data_member_location decl_file decl_line type
               DW_TAG_member
                Name      : _blksize
                Offset    : 1747
                CU offset : 1747
                Attrs     : name data_member_location decl_file decl_line type
               DW_TAG_member
                Name      : _old_offset
                Offset    : 1766
                CU offset : 1766
                Attrs     : name data_member_location decl_file decl_line type
               DW_TAG_member
                Name      : _cur_column
                Offset    : 1788
                CU offset : 1788
                Attrs     : name data_member_location decl_file decl_line type
               DW_TAG_member
                Name      : _vtable_offset
                Offset    : 1810
                CU offset : 1810
                Attrs     : name data_member_location decl_file decl_line type
               DW_TAG_member
                Name      : _shortbuf
                Offset    : 1835
                CU offset : 1835
                Attrs     : name data_member_location decl_file decl_line type
               DW_TAG_member
                Name      : _lock
                Offset    : 1855
                CU offset : 1855
                Attrs     : name data_member_location decl_file decl_line type
               DW_TAG_member
                Name      : _offset
                Offset    : 1871
                CU offset : 1871
                Attrs     : name data_member_location decl_file decl_line type
               DW_TAG_member
                Name      : _unused2
                Offset    : 1889
                CU offset : 1889
                Attrs     : name data_member_location decl_file decl_line type
          DW_TAG_structure_type
           Name      : _IO_marker
           Offset    : 1909
           CU offset : 1909
           Attrs     : sibling name byte_size decl_file decl_line
           byte size : 12
               DW_TAG_member
                Name      : _next
                Offset    : 1928
                CU offset : 1928
                Attrs     : name data_member_location decl_file decl_line type
               DW_TAG_member
                Name      : _sbuf
                Offset    : 1944
                CU offset : 1944
                Attrs     : name data_member_location decl_file decl_line type
               DW_TAG_member
                Name      : _pos
                Offset    : 1960
                CU offset : 1960
                Attrs     : name data_member_location decl_file decl_line type
          DW_TAG_pointer_type
           Name      : * NO NAME *
           Offset    : 1976
           CU offset : 1976
           Attrs     : byte_size type
           byte size : 4
          DW_TAG_pointer_type
           Name      : * NO NAME *
           Offset    : 1982
           CU offset : 1982
           Attrs     : byte_size type
           byte size : 4
          DW_TAG_array_type
           Name      : * NO NAME *
           Offset    : 1988
           CU offset : 1988
           Attrs     : sibling type
               DW_TAG_subrange_type
                Name      : * NO NAME *
                Offset    : 1997
                CU offset : 1997
                Attrs     : upper_bound type
          DW_TAG_pointer_type
           Name      : * NO NAME *
           Offset    : 2004
           CU offset : 2004
           Attrs     : byte_size
           byte size : 4
          DW_TAG_array_type
           Name      : * NO NAME *
           Offset    : 2006
           CU offset : 2006
           Attrs     : sibling type
               DW_TAG_subrange_type
                Name      : * NO NAME *
                Offset    : 2015
                CU offset : 2015
                Attrs     : upper_bound type
          DW_TAG_typedef
           Name      : FILE
           Offset    : 2022
           CU offset : 2022
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : wchar_t
           Offset    : 2034
           CU offset : 2034
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : wint_t
           Offset    : 2050
           CU offset : 2050
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : _G_int16_t
           Offset    : 2065
           CU offset : 2065
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : _G_int32_t
           Offset    : 2083
           CU offset : 2083
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : _G_uint16_t
           Offset    : 2101
           CU offset : 2101
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : _G_uint32_t
           Offset    : 2120
           CU offset : 2120
           Attrs     : name decl_file decl_line type
          DW_TAG_structure_type
           Name      : _IO_jump_t
           Offset    : 2139
           CU offset : 2139
           Attrs     : name declaration
          DW_TAG_typedef
           Name      : _IO_lock_t
           Offset    : 2152
           CU offset : 2152
           Attrs     : name decl_file decl_line
          DW_TAG_typedef
           Name      : _IO_FILE
           Offset    : 2166
           CU offset : 2166
           Attrs     : name decl_file decl_line type
          DW_TAG_structure_type
           Name      : _IO_FILE_plus
           Offset    : 2182
           CU offset : 2182
           Attrs     : name declaration
          DW_TAG_typedef
           Name      : __io_read_fn
           Offset    : 2198
           CU offset : 2198
           Attrs     : name decl_file decl_line type
          DW_TAG_subroutine_type
           Name      : * NO NAME *
           Offset    : 2219
           CU offset : 2219
           Attrs     : sibling prototyped type
               DW_TAG_formal_parameter
                Name      : * NO NAME *
                Offset    : 2229
                CU offset : 2229
                Attrs     : type
               DW_TAG_formal_parameter
                Name      : * NO NAME *
                Offset    : 2234
                CU offset : 2234
                Attrs     : type
               DW_TAG_formal_parameter
                Name      : * NO NAME *
                Offset    : 2239
                CU offset : 2239
                Attrs     : type
          DW_TAG_typedef
           Name      : __io_write_fn
           Offset    : 2245
           CU offset : 2245
           Attrs     : name decl_file decl_line type
          DW_TAG_subroutine_type
           Name      : * NO NAME *
           Offset    : 2267
           CU offset : 2267
           Attrs     : sibling prototyped type
               DW_TAG_formal_parameter
                Name      : * NO NAME *
                Offset    : 2277
                CU offset : 2277
                Attrs     : type
               DW_TAG_formal_parameter
                Name      : * NO NAME *
                Offset    : 2282
                CU offset : 2282
                Attrs     : type
               DW_TAG_formal_parameter
                Name      : * NO NAME *
                Offset    : 2287
                CU offset : 2287
                Attrs     : type
          DW_TAG_pointer_type
           Name      : * NO NAME *
           Offset    : 2293
           CU offset : 2293
           Attrs     : byte_size type
           byte size : 4
          DW_TAG_const_type
           Name      : * NO NAME *
           Offset    : 2299
           CU offset : 2299
           Attrs     : type
          DW_TAG_typedef
           Name      : __io_seek_fn
           Offset    : 2304
           CU offset : 2304
           Attrs     : name decl_file decl_line type
          DW_TAG_subroutine_type
           Name      : * NO NAME *
           Offset    : 2325
           CU offset : 2325
           Attrs     : sibling prototyped type
               DW_TAG_formal_parameter
                Name      : * NO NAME *
                Offset    : 2335
                CU offset : 2335
                Attrs     : type
               DW_TAG_formal_parameter
                Name      : * NO NAME *
                Offset    : 2340
                CU offset : 2340
                Attrs     : type
               DW_TAG_formal_parameter
                Name      : * NO NAME *
                Offset    : 2345
                CU offset : 2345
                Attrs     : type
          DW_TAG_typedef
           Name      : __io_close_fn
           Offset    : 2351
           CU offset : 2351
           Attrs     : name decl_file decl_line type
          DW_TAG_subroutine_type
           Name      : * NO NAME *
           Offset    : 2373
           CU offset : 2373
           Attrs     : sibling prototyped type
               DW_TAG_formal_parameter
                Name      : * NO NAME *
                Offset    : 2383
                CU offset : 2383
                Attrs     : type
          DW_TAG_typedef
           Name      : fpos_t
           Offset    : 2389
           CU offset : 2389
           Attrs     : name decl_file decl_line type
          DW_TAG_typedef
           Name      : off_t
           Offset    : 2403
           CU offset : 2403
           Attrs     : name decl_file decl_line type
New CU: off = 2418, hsize = 11, ab = 213, as = 4, os = 4
     DW_TAG_compile_unit
      Name      : f.c
      Offset    : 2429
      CU offset : 11
      Attrs     : name stmt_list low_pc high_pc language comp_dir producer
      low PC    : 0x10000490
      high PC   : 0x100004b0
      language  : 1
      directory : /shoggoth/drepper
      producer  : GNU C 2.96-laurel-000912
          DW_TAG_subprogram
           Name      : foo
           Offset    : 2490
           CU offset : 72
           Attrs     : name low_pc high_pc prototyped decl_file decl_line external frame_base type
           low PC    : 0x10000490
           high PC   : 0x100004b0
          DW_TAG_base_type
           Name      : int
           Offset    : 2513
           CU offset : 95
           Attrs     : name byte_size encoding
           byte size : 4
New CU: off = 2521, hsize = 11, ab = 267, as = 4, os = 4
     DW_TAG_compile_unit
      Name      : m.c
      Offset    : 2532
      CU offset : 11
      Attrs     : name stmt_list low_pc high_pc language comp_dir producer
      low PC    : 0x100004b0
      high PC   : 0x10000514
      language  : 1
      directory : /shoggoth/drepper
      producer  : GNU C 2.96-laurel-000912
          DW_TAG_subprogram
           Name      : main
           Offset    : 2593
           CU offset : 72
           Attrs     : sibling name low_pc high_pc prototyped decl_file decl_line external frame_base type
           low PC    : 0x100004b0
           high PC   : 0x10000514
               DW_TAG_subprogram
                Name      : bar
                Offset    : 2621
                CU offset : 100
                Attrs     : sibling name decl_file decl_line declaration external type
                    DW_TAG_unspecified_parameters
                     Name      : * NO NAME *
                     Offset    : 2638
                     CU offset : 117
                     Attrs     :
               DW_TAG_subprogram
                Name      : foo
                Offset    : 2640
                CU offset : 119
                Attrs     : name decl_file decl_line declaration external type
                    DW_TAG_unspecified_parameters
                     Name      : * NO NAME *
                     Offset    : 2653
                     CU offset : 132
                     Attrs     :
          DW_TAG_base_type
           Name      : int
           Offset    : 2656
           CU offset : 135
           Attrs     : name byte_size encoding
           byte size : 4
          DW_TAG_variable
           Name      : a
           Offset    : 2663
           CU offset : 142
           Attrs     : location name decl_file decl_line external type
EOF

exit 0
