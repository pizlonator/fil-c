#! /bin/sh
# Copyright (C) 2017 Red Hat, Inc.
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

# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=77589
#
# program repro
#   type small_stride
#      character*40 long_string
#      integer      small_pad
#   end type small_stride
#   type(small_stride), dimension (20), target :: unpleasant
#   character*40, pointer, dimension(:):: c40pt
#   integer i
#   do i = 0,19
#      unpleasant(i+1)%small_pad = i+1
#      unpleasant(i+1)%long_string = char (ichar('0') + i) // '-hello'
#   end do
#   c40pt => unpleasant%long_string
#   print *, c40pt  ! break-here
# end program repro
#
# Needs GCC7+
# gfortran -o testfile-stridex dwarf-stridex.f90 -Wall -g

testfiles testfile-stridex

testrun_compare ${abs_top_builddir}/tests/varlocs --exprlocs -e testfile-stridex <<\EOF
module 'testfile-stridex'
[b] CU 'dwarf-stridex.f90'@400717
  producer (strp)
  language (data1)
  identifier_case (data1)
  name (strp)
  comp_dir (strp)
  low_pc (addr)
  high_pc (data8)
  stmt_list (sec_offset)
  [2e] base_type "integer(kind=8)"
    byte_size (data1)
    encoding (data1)
    name (strp)
  [35] structure_type "small_stride"
    name (strp)
    byte_size (data1)
    decl_file (data1)
    decl_line (data1)
    sibling (ref4)
    [41] member "long_string"
      name (strp)
      decl_file (data1)
      decl_line (data1)
      type (ref4)
      data_member_location (data1) {plus_uconst(0)}
    [4d] member "small_pad"
      name (strp)
      decl_file (data1)
      decl_line (data1)
      type (ref4)
      data_member_location (data1) {plus_uconst(40)}
  [5a] string_type
    byte_size (data1)
  [5c] base_type "integer(kind=4)"
    byte_size (data1)
    encoding (data1)
    name (strp)
  [63] const_type
    type (ref4)
  [68] subprogram "main"
    external (flag_present)
    name (strp)
    decl_file (data1)
    decl_line (data1)
    type (ref4)
    low_pc (addr)
    high_pc (data8)
    frame_base (exprloc) {call_frame_cfa {bregx(7,8)}}
    GNU_all_tail_call_sites (flag_present)
    sibling (ref4)
    [89] formal_parameter "argc"
      name (strp)
      decl_file (data1)
      decl_line (data1)
      type (ref4)
      location (exprloc) {fbreg(-20)}
    [97] formal_parameter "argv"
      name (strp)
      decl_file (data1)
      decl_line (data1)
      type (ref4)
      location (exprloc) {fbreg(-32), deref}
  [a7] pointer_type
    byte_size (data1)
    type (ref4)
  [ad] base_type "character(kind=1)"
    byte_size (data1)
    encoding (data1)
    name (strp)
  [b4] subprogram "repro"
    name (strp)
    decl_file (data1)
    decl_line (data1)
    main_subprogram (flag_present)
    calling_convention (data1)
    low_pc (addr)
    high_pc (data8)
    frame_base (exprloc) {call_frame_cfa {bregx(7,8)}}
    GNU_all_tail_call_sites (flag_present)
    sibling (ref4)
    [d2] variable "c40pt"
      name (strp)
      decl_file (data1)
      decl_line (data1)
      type (ref4)
      location (exprloc) {fbreg(-128)}
    [e1] variable "span.0"
      name (strp)
      type (ref4)
      artificial (flag_present)
      location (exprloc) {fbreg(-80)}
    [ee] variable "i"
      name (string)
      decl_file (data1)
      decl_line (data1)
      type (ref4)
      location (exprloc) {fbreg(-68)}
    [fb] variable "unpleasant"
      name (strp)
      decl_file (data1)
      decl_line (data1)
      type (ref4)
      location (exprloc) {fbreg(-1008)}
    [10a] lexical_block
      low_pc (addr)
      high_pc (data8)
      sibling (ref4)
      [11f] lexical_block
        low_pc (addr)
        high_pc (data8)
    [131] lexical_block
      low_pc (addr)
      high_pc (data8)
      [142] lexical_block
        low_pc (addr)
        high_pc (data8)
        [153] lexical_block
          low_pc (addr)
          high_pc (data8)
  [167] array_type
    data_location (exprloc) {push_object_address, deref}
    associated (exprloc) {push_object_address, deref, lit0, ne}
    type (ref4)
    sibling (ref4)
    [178] subrange_type
      lower_bound (exprloc) {push_object_address, plus_uconst(32), deref}
      upper_bound (exprloc) {push_object_address, plus_uconst(40), deref}
      byte_stride (exprloc) {push_object_address, plus_uconst(24), deref, GNU_variable_value([e1]) {fbreg(-80)}, mul}
  [18f] array_type
    type (ref4)
    [194] subrange_type
      type (ref4)
      upper_bound (sdata)
EOF

exit 0
