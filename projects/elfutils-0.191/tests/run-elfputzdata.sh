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

# Random ELF32 testfile
testfiles testfile4

testrun_compare ${abs_top_builddir}/tests/elfputzdata gnu testfile4 <<\EOF
Cannot compress 1 .interp
Cannot compress 2 .note.ABI-tag
Cannot compress 3 .hash
Cannot compress 4 .dynsym
Cannot compress 5 .dynstr
Cannot compress 6 .gnu.version
Cannot compress 7 .gnu.version_r
Cannot compress 8 .rel.got
Cannot compress 9 .rel.plt
Cannot compress 10 .init
Cannot compress 11 .plt
Cannot compress 12 .text
Cannot compress 13 .fini
Cannot compress 14 .rodata
Cannot compress 15 .data
Cannot compress 16 .eh_frame
Cannot compress 17 .gcc_except_table
Cannot compress 18 .ctors
Cannot compress 19 .dtors
Cannot compress 20 .got
Cannot compress 21 .dynamic
Lets compress 22 .sbss, size: 0
Cannot compress 23 .bss
Lets compress 24 .stab, size: 21540
Lets compress 25 .stabstr, size: 57297
Lets compress 26 .comment, size: 648
Lets compress 27 .debug_aranges, size: 56
Lets compress 28 .debug_pubnames, size: 93
Lets compress 29 .debug_info, size: 960
Lets compress 30 .debug_abbrev, size: 405
Lets compress 31 .debug_line, size: 189
Lets compress 32 .note, size: 240
Lets compress 33 .shstrtab, size: 320
Lets compress 34 .symtab, size: 5488
Lets compress 35 .strtab, size: 5727
EOF

testrun_compare ${abs_top_builddir}/tests/elfputzdata elf testfile4 <<\EOF
Cannot compress 1 .interp
Cannot compress 2 .note.ABI-tag
Cannot compress 3 .hash
Cannot compress 4 .dynsym
Cannot compress 5 .dynstr
Cannot compress 6 .gnu.version
Cannot compress 7 .gnu.version_r
Cannot compress 8 .rel.got
Cannot compress 9 .rel.plt
Cannot compress 10 .init
Cannot compress 11 .plt
Cannot compress 12 .text
Cannot compress 13 .fini
Cannot compress 14 .rodata
Cannot compress 15 .data
Cannot compress 16 .eh_frame
Cannot compress 17 .gcc_except_table
Cannot compress 18 .ctors
Cannot compress 19 .dtors
Cannot compress 20 .got
Cannot compress 21 .dynamic
Lets compress 22 .sbss, size: 0
Cannot compress 23 .bss
Lets compress 24 .stab, size: 21540
Lets compress 25 .stabstr, size: 57297
Lets compress 26 .comment, size: 648
Lets compress 27 .debug_aranges, size: 56
Lets compress 28 .debug_pubnames, size: 93
Lets compress 29 .debug_info, size: 960
Lets compress 30 .debug_abbrev, size: 405
Lets compress 31 .debug_line, size: 189
Lets compress 32 .note, size: 240
Lets compress 33 .shstrtab, size: 320
Lets compress 34 .symtab, size: 5488
Lets compress 35 .strtab, size: 5727
EOF

# Random ELF64 testfile
testfiles testfile12

testrun_compare ${abs_top_builddir}/tests/elfputzdata gnu testfile12 <<\EOF
Cannot compress 1 .hash
Cannot compress 2 .dynsym
Cannot compress 3 .dynstr
Cannot compress 4 .gnu.version
Cannot compress 5 .gnu.version_r
Cannot compress 6 .rela.dyn
Cannot compress 7 .rela.plt
Cannot compress 8 .init
Cannot compress 9 .plt
Cannot compress 10 .text
Cannot compress 11 .fini
Cannot compress 12 .rodata
Cannot compress 13 .eh_frame_hdr
Cannot compress 14 .eh_frame
Cannot compress 15 .data
Cannot compress 16 .dynamic
Cannot compress 17 .ctors
Cannot compress 18 .dtors
Cannot compress 19 .jcr
Cannot compress 20 .got
Cannot compress 21 .bss
Lets compress 22 .comment, size: 246
Lets compress 23 .debug_aranges, size: 192
Lets compress 24 .debug_pubnames, size: 26
Lets compress 25 .debug_info, size: 3468
Lets compress 26 .debug_abbrev, size: 341
Lets compress 27 .debug_line, size: 709
Lets compress 28 .debug_frame, size: 56
Lets compress 29 .debug_str, size: 2235
Lets compress 30 .debug_macinfo, size: 10518
Lets compress 31 .shstrtab, size: 308
Lets compress 32 .symtab, size: 1944
Lets compress 33 .strtab, size: 757
EOF

testrun_compare ${abs_top_builddir}/tests/elfputzdata elf testfile12 <<\EOF
Cannot compress 1 .hash
Cannot compress 2 .dynsym
Cannot compress 3 .dynstr
Cannot compress 4 .gnu.version
Cannot compress 5 .gnu.version_r
Cannot compress 6 .rela.dyn
Cannot compress 7 .rela.plt
Cannot compress 8 .init
Cannot compress 9 .plt
Cannot compress 10 .text
Cannot compress 11 .fini
Cannot compress 12 .rodata
Cannot compress 13 .eh_frame_hdr
Cannot compress 14 .eh_frame
Cannot compress 15 .data
Cannot compress 16 .dynamic
Cannot compress 17 .ctors
Cannot compress 18 .dtors
Cannot compress 19 .jcr
Cannot compress 20 .got
Cannot compress 21 .bss
Lets compress 22 .comment, size: 246
Lets compress 23 .debug_aranges, size: 192
Lets compress 24 .debug_pubnames, size: 26
Lets compress 25 .debug_info, size: 3468
Lets compress 26 .debug_abbrev, size: 341
Lets compress 27 .debug_line, size: 709
Lets compress 28 .debug_frame, size: 56
Lets compress 29 .debug_str, size: 2235
Lets compress 30 .debug_macinfo, size: 10518
Lets compress 31 .shstrtab, size: 308
Lets compress 32 .symtab, size: 1944
Lets compress 33 .strtab, size: 757
EOF

# Random ELF64BE testfile
testfiles testfileppc64

testrun_compare ${abs_top_builddir}/tests/elfputzdata gnu testfileppc64 <<\EOF
Cannot compress 1 .interp
Cannot compress 2 .note.ABI-tag
Cannot compress 3 .note.gnu.build-id
Cannot compress 4 .gnu.hash
Cannot compress 5 .dynsym
Cannot compress 6 .dynstr
Cannot compress 7 .gnu.version
Cannot compress 8 .gnu.version_r
Cannot compress 9 .rela.plt
Cannot compress 10 .init
Cannot compress 11 .text
Cannot compress 12 .fini
Cannot compress 13 .rodata
Cannot compress 14 .eh_frame_hdr
Cannot compress 15 .eh_frame
Cannot compress 16 .init_array
Cannot compress 17 .fini_array
Cannot compress 18 .jcr
Cannot compress 19 .dynamic
Cannot compress 20 .data
Cannot compress 21 .opd
Cannot compress 22 .got
Cannot compress 23 .plt
Cannot compress 24 .bss
Lets compress 25 .comment, size: 88
Lets compress 26 .debug_aranges, size: 96
Lets compress 27 .debug_info, size: 363
Lets compress 28 .debug_abbrev, size: 315
Lets compress 29 .debug_line, size: 119
Lets compress 30 .debug_frame, size: 96
Lets compress 31 .debug_str, size: 174
Lets compress 32 .debug_loc, size: 171
Lets compress 33 .debug_ranges, size: 32
Lets compress 34 .shstrtab, size: 352
Lets compress 35 .symtab, size: 1800
Lets compress 36 .strtab, size: 602
EOF

testrun_compare ${abs_top_builddir}/tests/elfputzdata elf testfileppc64 <<\EOF
Cannot compress 1 .interp
Cannot compress 2 .note.ABI-tag
Cannot compress 3 .note.gnu.build-id
Cannot compress 4 .gnu.hash
Cannot compress 5 .dynsym
Cannot compress 6 .dynstr
Cannot compress 7 .gnu.version
Cannot compress 8 .gnu.version_r
Cannot compress 9 .rela.plt
Cannot compress 10 .init
Cannot compress 11 .text
Cannot compress 12 .fini
Cannot compress 13 .rodata
Cannot compress 14 .eh_frame_hdr
Cannot compress 15 .eh_frame
Cannot compress 16 .init_array
Cannot compress 17 .fini_array
Cannot compress 18 .jcr
Cannot compress 19 .dynamic
Cannot compress 20 .data
Cannot compress 21 .opd
Cannot compress 22 .got
Cannot compress 23 .plt
Cannot compress 24 .bss
Lets compress 25 .comment, size: 88
Lets compress 26 .debug_aranges, size: 96
Lets compress 27 .debug_info, size: 363
Lets compress 28 .debug_abbrev, size: 315
Lets compress 29 .debug_line, size: 119
Lets compress 30 .debug_frame, size: 96
Lets compress 31 .debug_str, size: 174
Lets compress 32 .debug_loc, size: 171
Lets compress 33 .debug_ranges, size: 32
Lets compress 34 .shstrtab, size: 352
Lets compress 35 .symtab, size: 1800
Lets compress 36 .strtab, size: 602
EOF

# Random ELF32BE testfile
testfiles testfileppc32

testrun_compare ${abs_top_builddir}/tests/elfputzdata gnu testfileppc32 <<\EOF
Cannot compress 1 .interp
Cannot compress 2 .note.ABI-tag
Cannot compress 3 .note.gnu.build-id
Cannot compress 4 .gnu.hash
Cannot compress 5 .dynsym
Cannot compress 6 .dynstr
Cannot compress 7 .gnu.version
Cannot compress 8 .gnu.version_r
Cannot compress 9 .rela.dyn
Cannot compress 10 .rela.plt
Cannot compress 11 .init
Cannot compress 12 .text
Cannot compress 13 .fini
Cannot compress 14 .rodata
Cannot compress 15 .eh_frame_hdr
Cannot compress 16 .eh_frame
Cannot compress 17 .init_array
Cannot compress 18 .fini_array
Cannot compress 19 .jcr
Cannot compress 20 .got2
Cannot compress 21 .dynamic
Cannot compress 22 .got
Cannot compress 23 .plt
Cannot compress 24 .data
Cannot compress 25 .sdata
Cannot compress 26 .bss
Lets compress 27 .comment, size: 88
Lets compress 28 .debug_aranges, size: 64
Lets compress 29 .debug_info, size: 319
Lets compress 30 .debug_abbrev, size: 318
Lets compress 31 .debug_line, size: 109
Lets compress 32 .debug_frame, size: 64
Lets compress 33 .debug_str, size: 179
Lets compress 34 .debug_loc, size: 99
Lets compress 35 .debug_ranges, size: 16
Lets compress 36 .shstrtab, size: 370
Lets compress 37 .symtab, size: 1232
Lets compress 38 .strtab, size: 569
EOF

testrun_compare ${abs_top_builddir}/tests/elfputzdata elf testfileppc32 <<\EOF
Cannot compress 1 .interp
Cannot compress 2 .note.ABI-tag
Cannot compress 3 .note.gnu.build-id
Cannot compress 4 .gnu.hash
Cannot compress 5 .dynsym
Cannot compress 6 .dynstr
Cannot compress 7 .gnu.version
Cannot compress 8 .gnu.version_r
Cannot compress 9 .rela.dyn
Cannot compress 10 .rela.plt
Cannot compress 11 .init
Cannot compress 12 .text
Cannot compress 13 .fini
Cannot compress 14 .rodata
Cannot compress 15 .eh_frame_hdr
Cannot compress 16 .eh_frame
Cannot compress 17 .init_array
Cannot compress 18 .fini_array
Cannot compress 19 .jcr
Cannot compress 20 .got2
Cannot compress 21 .dynamic
Cannot compress 22 .got
Cannot compress 23 .plt
Cannot compress 24 .data
Cannot compress 25 .sdata
Cannot compress 26 .bss
Lets compress 27 .comment, size: 88
Lets compress 28 .debug_aranges, size: 64
Lets compress 29 .debug_info, size: 319
Lets compress 30 .debug_abbrev, size: 318
Lets compress 31 .debug_line, size: 109
Lets compress 32 .debug_frame, size: 64
Lets compress 33 .debug_str, size: 179
Lets compress 34 .debug_loc, size: 99
Lets compress 35 .debug_ranges, size: 16
Lets compress 36 .shstrtab, size: 370
Lets compress 37 .symtab, size: 1232
Lets compress 38 .strtab, size: 569
EOF

exit 0
