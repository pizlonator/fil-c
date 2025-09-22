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

# On Fedora 31 with GCC 9.2.1 and binutils 2.32-31
# echo "int main () { }" | \
# gcc -o testfile_pt_gnu_prop \
#     -Os -fstack-clash-protection -fcf-protection=full -xc - && \
# eu-strip --remove-comment -R .gnu.build.attributes testfile_pt_gnu_prop
#
# echo "int main () { }" | \
# gcc -m32 -o testfile_pt_gnu_prop \
#     -Os -fstack-clash-protection -fcf-protection=full -xc - && \
# eu-strip --remove-comment -R .gnu.build.attributes testfile_pt_gnu_prop32

testfiles testfile_pt_gnu_prop testfile_pt_gnu_prop32

testrun_compare ${abs_top_builddir}/src/readelf -ln testfile_pt_gnu_prop32 <<\EOF
Program Headers:
  Type           Offset   VirtAddr   PhysAddr   FileSiz  MemSiz   Flg Align
  PHDR           0x000034 0x08048034 0x08048034 0x000180 0x000180 R   0x4
  INTERP         0x0001b4 0x080481b4 0x080481b4 0x000013 0x000013 R   0x1
	[Requesting program interpreter: /lib/ld-linux.so.2]
  LOAD           0x000000 0x08048000 0x08048000 0x000308 0x000308 R   0x1000
  LOAD           0x001000 0x08049000 0x08049000 0x000224 0x000224 R E 0x1000
  LOAD           0x002000 0x0804a000 0x0804a000 0x00015c 0x00015c R   0x1000
  LOAD           0x002f0c 0x0804bf0c 0x0804bf0c 0x000108 0x00010c RW  0x1000
  DYNAMIC        0x002f14 0x0804bf14 0x0804bf14 0x0000e8 0x0000e8 RW  0x4
  NOTE           0x0001c8 0x080481c8 0x080481c8 0x000060 0x000060 R   0x4
  GNU_PROPERTY   0x0001ec 0x080481ec 0x080481ec 0x00001c 0x00001c R   0x4
  GNU_EH_FRAME   0x00200c 0x0804a00c 0x0804a00c 0x00004c 0x00004c R   0x4
  GNU_STACK      0x000000 0x00000000 0x00000000 0x000000 0x000000 RW  0x10
  GNU_RELRO      0x002f0c 0x0804bf0c 0x0804bf0c 0x0000f4 0x0000f4 R   0x1

 Section to Segment mapping:
  Segment Sections...
   00     
   01      [RO: .interp]
   02      [RO: .interp .note.gnu.build-id .note.gnu.property .note.ABI-tag .gnu.hash .dynsym .dynstr .gnu.version .gnu.version_r .rel.dyn .rel.plt]
   03      [RO: .init .plt .plt.sec .text .fini]
   04      [RO: .rodata .eh_frame_hdr .eh_frame]
   05      [RELRO: .init_array .fini_array .dynamic .got] .got.plt .data .bss
   06      [RELRO: .dynamic]
   07      [RO: .note.gnu.build-id .note.gnu.property .note.ABI-tag]
   08      [RO: .note.gnu.property]
   09      [RO: .eh_frame_hdr]
   10     
   11      [RELRO: .init_array .fini_array .dynamic .got]

Note section [ 2] '.note.gnu.build-id' of 36 bytes at offset 0x1c8:
  Owner          Data size  Type
  GNU                   20  GNU_BUILD_ID
    Build ID: 2fcce91f5c2532f78b00a9f5f565354d2f44bc19

Note section [ 3] '.note.gnu.property' of 28 bytes at offset 0x1ec:
  Owner          Data size  Type
  GNU                   12  GNU_PROPERTY_TYPE_0
    X86 FEATURE_1_AND: 00000003 IBT SHSTK

Note section [ 4] '.note.ABI-tag' of 32 bytes at offset 0x208:
  Owner          Data size  Type
  GNU                   16  GNU_ABI_TAG
    OS: Linux, ABI: 3.2.0
EOF

testrun ${abs_top_builddir}/src/elflint --gnu testfile_pt_gnu_prop32

testrun_compare ${abs_top_builddir}/src/readelf -ln testfile_pt_gnu_prop <<\EOF
Program Headers:
  Type           Offset   VirtAddr           PhysAddr           FileSiz  MemSiz   Flg Align
  PHDR           0x000040 0x0000000000400040 0x0000000000400040 0x0002d8 0x0002d8 R   0x8
  INTERP         0x000318 0x0000000000400318 0x0000000000400318 0x00001c 0x00001c R   0x1
	[Requesting program interpreter: /lib64/ld-linux-x86-64.so.2]
  LOAD           0x000000 0x0000000000400000 0x0000000000400000 0x000498 0x000498 R   0x1000
  LOAD           0x001000 0x0000000000401000 0x0000000000401000 0x0001a5 0x0001a5 R E 0x1000
  LOAD           0x002000 0x0000000000402000 0x0000000000402000 0x000100 0x000100 R   0x1000
  LOAD           0x002e50 0x0000000000403e50 0x0000000000403e50 0x0001cc 0x0001d0 RW  0x1000
  DYNAMIC        0x002e60 0x0000000000403e60 0x0000000000403e60 0x000190 0x000190 RW  0x8
  NOTE           0x000338 0x0000000000400338 0x0000000000400338 0x000020 0x000020 R   0x8
  NOTE           0x000358 0x0000000000400358 0x0000000000400358 0x000044 0x000044 R   0x4
  GNU_PROPERTY   0x000338 0x0000000000400338 0x0000000000400338 0x000020 0x000020 R   0x8
  GNU_EH_FRAME   0x002010 0x0000000000402010 0x0000000000402010 0x000034 0x000034 R   0x4
  GNU_STACK      0x000000 0x0000000000000000 0x0000000000000000 0x000000 0x000000 RW  0x10
  GNU_RELRO      0x002e50 0x0000000000403e50 0x0000000000403e50 0x0001b0 0x0001b0 R   0x1

 Section to Segment mapping:
  Segment Sections...
   00     
   01      [RO: .interp]
   02      [RO: .interp .note.gnu.property .note.gnu.build-id .note.ABI-tag .gnu.hash .dynsym .dynstr .gnu.version .gnu.version_r .rela.dyn]
   03      [RO: .init .text .fini]
   04      [RO: .rodata .eh_frame_hdr .eh_frame]
   05      [RELRO: .init_array .fini_array .dynamic .got] .got.plt .data .bss
   06      [RELRO: .dynamic]
   07      [RO: .note.gnu.property]
   08      [RO: .note.gnu.build-id .note.ABI-tag]
   09      [RO: .note.gnu.property]
   10      [RO: .eh_frame_hdr]
   11     
   12      [RELRO: .init_array .fini_array .dynamic .got]

Note section [ 2] '.note.gnu.property' of 32 bytes at offset 0x338:
  Owner          Data size  Type
  GNU                   16  GNU_PROPERTY_TYPE_0
    X86 FEATURE_1_AND: 00000003 IBT SHSTK

Note section [ 3] '.note.gnu.build-id' of 36 bytes at offset 0x358:
  Owner          Data size  Type
  GNU                   20  GNU_BUILD_ID
    Build ID: 84fa4d40bad074bc82431575821902da624a5b22

Note section [ 4] '.note.ABI-tag' of 32 bytes at offset 0x37c:
  Owner          Data size  Type
  GNU                   16  GNU_ABI_TAG
    OS: Linux, ABI: 3.2.0
EOF

testrun ${abs_top_builddir}/src/elflint --gnu testfile_pt_gnu_prop

exit 0
