#! /bin/sh
# Copyright (C) 2020 Red Hat, Inc.
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

# See run-readelf-n.sh
testfiles testfile-gnu-property-note-aarch64

testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=frames testfile-gnu-property-note-aarch64 <<\EOF

Call frame search table section [17] '.eh_frame_hdr':
 version:          1
 eh_frame_ptr_enc: 0x1b (sdata4 pcrel)
 fde_count_enc:    0x3 (udata4)
 table_enc:        0x3b (sdata4 datarel)
 eh_frame_ptr:     0x44 (offset: 0x758)
 fde_count:        7
 Table:
  0xfffffe70 (offset:  0x580) -> 0x5c fde=[    14]
  0xfffffea0 (offset:  0x5b0) -> 0x70 fde=[    28]
  0xfffffee0 (offset:  0x5f0) -> 0x84 fde=[    3c]
  0xffffff20 (offset:  0x630) -> 0xac fde=[    64]
  0xffffff28 (offset:  0x638) -> 0xc0 fde=[    78]
  0xffffff40 (offset:  0x650) -> 0xd8 fde=[    90]
  0xffffffc0 (offset:  0x6d0) -> 0x110 fde=[    c8]

Call frame information section [18] '.eh_frame' at offset 0x758:

 [     0] CIE length=16
   CIE_id:                   0
   version:                  1
   augmentation:             "zR"
   code_alignment_factor:    4
   data_alignment_factor:    -8
   return_address_register:  30
   Augmentation data:        0x1b (FDE address encoding: sdata4 pcrel)

   Program:
     def_cfa r31 (sp) at offset 0

 [    14] FDE length=16 cie=[     0]
   CIE_pointer:              24
   initial_location:         0x0000000000400580 (offset: 0x580)
   address_range:            0x30 (end offset: 0x5b0)

   Program:
     nop
     nop
     nop

 [    28] FDE length=16 cie=[     0]
   CIE_pointer:              44
   initial_location:         0x00000000004005b0 (offset: 0x5b0)
   address_range:            0x3c (end offset: 0x5ec)

   Program:
     nop
     nop
     nop

 [    3c] FDE length=36 cie=[     0]
   CIE_pointer:              64
   initial_location:         0x00000000004005f0 (offset: 0x5f0)
   address_range:            0x38 (end offset: 0x628)

   Program:
     advance_loc 1 to 0x5f4
     AARCH64_negate_ra_state
     advance_loc 1 to 0x5f8
     def_cfa_offset 32
     offset r29 (x29) at cfa-32
     offset r30 (x30) at cfa-24
     advance_loc 2 to 0x600
     offset r19 (x19) at cfa-16
     advance_loc 8 to 0x620
     restore r30 (x30)
     restore r29 (x29)
     restore r19 (x19)
     def_cfa_offset 0
     advance_loc 1 to 0x624
     AARCH64_negate_ra_state
     nop
     nop
     nop

 [    64] FDE length=16 cie=[     0]
   CIE_pointer:              104
   initial_location:         0x0000000000400630 (offset: 0x630)
   address_range:            0x8 (end offset: 0x638)

   Program:
     nop
     nop
     nop

 [    78] FDE length=20 cie=[     0]
   CIE_pointer:              124
   initial_location:         0x0000000000400638 (offset: 0x638)
   address_range:            0xc (end offset: 0x644)

   Program:
     nop
     nop
     nop
     nop
     nop
     nop
     nop

 [    90] FDE length=52 cie=[     0]
   CIE_pointer:              148
   initial_location:         0x0000000000400650 (offset: 0x650)
   address_range:            0x80 (end offset: 0x6d0)

   Program:
     advance_loc 1 to 0x654
     AARCH64_negate_ra_state
     advance_loc 1 to 0x658
     def_cfa_offset 64
     offset r29 (x29) at cfa-64
     offset r30 (x30) at cfa-56
     advance_loc 2 to 0x660
     offset r19 (x19) at cfa-48
     offset r20 (x20) at cfa-40
     advance_loc 3 to 0x66c
     offset r21 (x21) at cfa-32
     offset r22 (x22) at cfa-24
     advance_loc 5 to 0x680
     offset r23 (x23) at cfa-16
     offset r24 (x24) at cfa-8
     advance_loc 18 to 0x6c8
     restore r30 (x30)
     restore r29 (x29)
     restore r23 (x23)
     restore r24 (x24)
     restore r21 (x21)
     restore r22 (x22)
     restore r19 (x19)
     restore r20 (x20)
     def_cfa_offset 0
     advance_loc 1 to 0x6cc
     AARCH64_negate_ra_state
     nop
     nop

 [    c8] FDE length=16 cie=[     0]
   CIE_pointer:              204
   initial_location:         0x00000000004006d0 (offset: 0x6d0)
   address_range:            0x8 (end offset: 0x6d8)

   Program:
     nop
     nop
     nop

 [    dc] Zero terminator
EOF

exit 0
