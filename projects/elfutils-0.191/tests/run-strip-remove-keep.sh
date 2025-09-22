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

# strip -o output and -f debug files
tempfiles testfile.elf testfile.debug

# A random 32bit testfile
testfiles testfile

# Explicitly keep .strtab (but not .symtab, so .strtab will be in both). 32bit
echo strip --keep-section=.strtab testfile
testrun ${abs_top_builddir}/src/strip --keep-section=.strtab -o testfile.elf -f testfile.debug testfile
echo elflint testfile.elf
testrun ${abs_top_builddir}/src/elflint --gnu testfile.elf
echo elflint testfile.debug
testrun ${abs_top_builddir}/src/elflint --gnu -d testfile.debug
echo readelf testfile.elf
testrun_compare ${abs_top_builddir}/src/readelf -S testfile.elf <<\EOF
There are 27 section headers, starting at offset 0xaf8:

Section Headers:
[Nr] Name                 Type         Addr     Off    Size   ES Flags Lk Inf Al
[ 0]                      NULL         00000000 000000 000000  0        0   0  0
[ 1] .interp              PROGBITS     080480f4 0000f4 000013  0 A      0   0  1
[ 2] .note.ABI-tag        NOTE         08048108 000108 000020  0 A      0   0  4
[ 3] .hash                HASH         08048128 000128 000030  4 A      4   0  4
[ 4] .dynsym              DYNSYM       08048158 000158 000070 16 A      5   1  4
[ 5] .dynstr              STRTAB       080481c8 0001c8 00008e  0 A      0   0  1
[ 6] .gnu.version         GNU_versym   08048256 000256 00000e  2 A      4   0  2
[ 7] .gnu.version_r       GNU_verneed  08048264 000264 000030  0 A      5   1  4
[ 8] .rel.got             REL          08048294 000294 000008  8 A      4  19  4
[ 9] .rel.plt             REL          0804829c 00029c 000020  8 A      4  11  4
[10] .init                PROGBITS     080482bc 0002bc 000018  0 AX     0   0  4
[11] .plt                 PROGBITS     080482d4 0002d4 000050  4 AX     0   0  4
[12] .text                PROGBITS     08048330 000330 00018c  0 AX     0   0 16
[13] .fini                PROGBITS     080484bc 0004bc 00001e  0 AX     0   0  4
[14] .rodata              PROGBITS     080484dc 0004dc 000008  0 A      0   0  4
[15] .data                PROGBITS     080494e4 0004e4 000010  0 WA     0   0  4
[16] .eh_frame            PROGBITS     080494f4 0004f4 000004  0 WA     0   0  4
[17] .ctors               PROGBITS     080494f8 0004f8 000008  0 WA     0   0  4
[18] .dtors               PROGBITS     08049500 000500 000008  0 WA     0   0  4
[19] .got                 PROGBITS     08049508 000508 000020  4 WA     0   0  4
[20] .dynamic             DYNAMIC      08049528 000528 0000a0  8 WA     5   0  4
[21] .bss                 NOBITS       080495c8 0005c8 00001c  0 WA     0   0  4
[22] .comment             PROGBITS     00000000 0005c8 000170  0        0   0  1
[23] .note                NOTE         00000000 000738 0000a0  0        0   0  1
[24] .strtab              STRTAB       00000000 0007d8 000235  0        0   0  1
[25] .gnu_debuglink       PROGBITS     00000000 000a10 000014  0        0   0  4
[26] .shstrtab            STRTAB       00000000 000a24 0000d1  0        0   0  1

EOF
echo readelf testfile.debug
testrun_compare ${abs_top_builddir}/src/readelf -S testfile.debug <<\EOF
There are 35 section headers, starting at offset 0x463c:

Section Headers:
[Nr] Name                 Type         Addr     Off    Size   ES Flags Lk Inf Al
[ 0]                      NULL         00000000 000000 000000  0        0   0  0
[ 1] .interp              NOBITS       080480f4 0000f4 000013  0 A      0   0  1
[ 2] .note.ABI-tag        NOTE         08048108 0000f4 000020  0 A      0   0  4
[ 3] .hash                NOBITS       08048128 000114 000030  4 A      4   0  4
[ 4] .dynsym              NOBITS       08048158 000114 000070 16 A      5   1  4
[ 5] .dynstr              NOBITS       080481c8 000114 00008e  0 A      0   0  1
[ 6] .gnu.version         NOBITS       08048256 000114 00000e  2 A      4   0  2
[ 7] .gnu.version_r       NOBITS       08048264 000114 000030  0 A      5   1  4
[ 8] .rel.got             NOBITS       08048294 000114 000008  8 A      4  19  4
[ 9] .rel.plt             NOBITS       0804829c 000114 000020  8 A      4  11  4
[10] .init                NOBITS       080482bc 000114 000018  0 AX     0   0  4
[11] .plt                 NOBITS       080482d4 000114 000050  4 AX     0   0  4
[12] .text                NOBITS       08048330 000120 00018c  0 AX     0   0 16
[13] .fini                NOBITS       080484bc 000120 00001e  0 AX     0   0  4
[14] .rodata              NOBITS       080484dc 000120 000008  0 A      0   0  4
[15] .data                NOBITS       080494e4 000120 000010  0 WA     0   0  4
[16] .eh_frame            NOBITS       080494f4 000120 000004  0 WA     0   0  4
[17] .ctors               NOBITS       080494f8 000120 000008  0 WA     0   0  4
[18] .dtors               NOBITS       08049500 000120 000008  0 WA     0   0  4
[19] .got                 NOBITS       08049508 000120 000020  4 WA     0   0  4
[20] .dynamic             NOBITS       08049528 000120 0000a0  8 WA     5   0  4
[21] .sbss                PROGBITS     080495c8 000120 000000  0 W      0   0  1
[22] .bss                 NOBITS       080495c8 000120 00001c  0 WA     0   0  4
[23] .stab                PROGBITS     00000000 000120 000720 12       24   0  4
[24] .stabstr             STRTAB       00000000 000840 001934  0        0   0  1
[25] .comment             NOBITS       00000000 002174 000170  0        0   0  1
[26] .debug_aranges       PROGBITS     00000000 002174 000060  0        0   0  1
[27] .debug_pubnames      PROGBITS     00000000 0021d4 000055  0        0   0  1
[28] .debug_info          PROGBITS     00000000 002229 001678  0        0   0  1
[29] .debug_abbrev        PROGBITS     00000000 0038a1 0001d2  0        0   0  1
[30] .debug_line          PROGBITS     00000000 003a73 000223  0        0   0  1
[31] .note                NOTE         00000000 003c96 0000a0  0        0   0  1
[32] .shstrtab            STRTAB       00000000 003d36 00012e  0        0   0  1
[33] .symtab              SYMTAB       00000000 003e64 0005a0 16       34  68  4
[34] .strtab              STRTAB       00000000 004404 000235  0        0   0  1

EOF

# Explicitly keep .symtab (pulls in .strtab, so they will both be in elf). 32bit
echo strip --keep-section=.symtab testfile
testrun ${abs_top_builddir}/src/strip --keep-section=.symtab -o testfile.elf -f testfile.debug testfile
echo elflint testfile.elf
testrun ${abs_top_builddir}/src/elflint --gnu testfile.elf
echo elflint testfile.debug
testrun ${abs_top_builddir}/src/elflint --gnu -d testfile.debug
echo readelf testfile.elf
testrun_compare ${abs_top_builddir}/src/readelf -S testfile.elf <<\EOF
There are 28 section headers, starting at offset 0x1010:

Section Headers:
[Nr] Name                 Type         Addr     Off    Size   ES Flags Lk Inf Al
[ 0]                      NULL         00000000 000000 000000  0        0   0  0
[ 1] .interp              PROGBITS     080480f4 0000f4 000013  0 A      0   0  1
[ 2] .note.ABI-tag        NOTE         08048108 000108 000020  0 A      0   0  4
[ 3] .hash                HASH         08048128 000128 000030  4 A      4   0  4
[ 4] .dynsym              DYNSYM       08048158 000158 000070 16 A      5   1  4
[ 5] .dynstr              STRTAB       080481c8 0001c8 00008e  0 A      0   0  1
[ 6] .gnu.version         GNU_versym   08048256 000256 00000e  2 A      4   0  2
[ 7] .gnu.version_r       GNU_verneed  08048264 000264 000030  0 A      5   1  4
[ 8] .rel.got             REL          08048294 000294 000008  8 A      4  19  4
[ 9] .rel.plt             REL          0804829c 00029c 000020  8 A      4  11  4
[10] .init                PROGBITS     080482bc 0002bc 000018  0 AX     0   0  4
[11] .plt                 PROGBITS     080482d4 0002d4 000050  4 AX     0   0  4
[12] .text                PROGBITS     08048330 000330 00018c  0 AX     0   0 16
[13] .fini                PROGBITS     080484bc 0004bc 00001e  0 AX     0   0  4
[14] .rodata              PROGBITS     080484dc 0004dc 000008  0 A      0   0  4
[15] .data                PROGBITS     080494e4 0004e4 000010  0 WA     0   0  4
[16] .eh_frame            PROGBITS     080494f4 0004f4 000004  0 WA     0   0  4
[17] .ctors               PROGBITS     080494f8 0004f8 000008  0 WA     0   0  4
[18] .dtors               PROGBITS     08049500 000500 000008  0 WA     0   0  4
[19] .got                 PROGBITS     08049508 000508 000020  4 WA     0   0  4
[20] .dynamic             DYNAMIC      08049528 000528 0000a0  8 WA     5   0  4
[21] .bss                 NOBITS       080495c8 0005c8 00001c  0 WA     0   0  4
[22] .comment             PROGBITS     00000000 0005c8 000170  0        0   0  1
[23] .note                NOTE         00000000 000738 0000a0  0        0   0  1
[24] .symtab              SYMTAB       00000000 0007d8 000510 16       25  59  4
[25] .strtab              STRTAB       00000000 000ce8 000235  0        0   0  1
[26] .gnu_debuglink       PROGBITS     00000000 000f20 000014  0        0   0  4
[27] .shstrtab            STRTAB       00000000 000f34 0000d9  0        0   0  1

EOF
echo readelf testfile.debug
testrun_compare ${abs_top_builddir}/src/readelf -S testfile.debug <<\EOF
There are 35 section headers, starting at offset 0x3e64:

Section Headers:
[Nr] Name                 Type         Addr     Off    Size   ES Flags Lk Inf Al
[ 0]                      NULL         00000000 000000 000000  0        0   0  0
[ 1] .interp              NOBITS       080480f4 0000f4 000013  0 A      0   0  1
[ 2] .note.ABI-tag        NOTE         08048108 0000f4 000020  0 A      0   0  4
[ 3] .hash                NOBITS       08048128 000114 000030  4 A      4   0  4
[ 4] .dynsym              NOBITS       08048158 000114 000070 16 A      5   1  4
[ 5] .dynstr              NOBITS       080481c8 000114 00008e  0 A      0   0  1
[ 6] .gnu.version         NOBITS       08048256 000114 00000e  2 A      4   0  2
[ 7] .gnu.version_r       NOBITS       08048264 000114 000030  0 A      5   1  4
[ 8] .rel.got             NOBITS       08048294 000114 000008  8 A      4  19  4
[ 9] .rel.plt             NOBITS       0804829c 000114 000020  8 A      4  11  4
[10] .init                NOBITS       080482bc 000114 000018  0 AX     0   0  4
[11] .plt                 NOBITS       080482d4 000114 000050  4 AX     0   0  4
[12] .text                NOBITS       08048330 000120 00018c  0 AX     0   0 16
[13] .fini                NOBITS       080484bc 000120 00001e  0 AX     0   0  4
[14] .rodata              NOBITS       080484dc 000120 000008  0 A      0   0  4
[15] .data                NOBITS       080494e4 000120 000010  0 WA     0   0  4
[16] .eh_frame            NOBITS       080494f4 000120 000004  0 WA     0   0  4
[17] .ctors               NOBITS       080494f8 000120 000008  0 WA     0   0  4
[18] .dtors               NOBITS       08049500 000120 000008  0 WA     0   0  4
[19] .got                 NOBITS       08049508 000120 000020  4 WA     0   0  4
[20] .dynamic             NOBITS       08049528 000120 0000a0  8 WA     5   0  4
[21] .sbss                PROGBITS     080495c8 000120 000000  0 W      0   0  1
[22] .bss                 NOBITS       080495c8 000120 00001c  0 WA     0   0  4
[23] .stab                PROGBITS     00000000 000120 000720 12       24   0  4
[24] .stabstr             STRTAB       00000000 000840 001934  0        0   0  1
[25] .comment             NOBITS       00000000 002174 000170  0        0   0  1
[26] .debug_aranges       PROGBITS     00000000 002174 000060  0        0   0  1
[27] .debug_pubnames      PROGBITS     00000000 0021d4 000055  0        0   0  1
[28] .debug_info          PROGBITS     00000000 002229 001678  0        0   0  1
[29] .debug_abbrev        PROGBITS     00000000 0038a1 0001d2  0        0   0  1
[30] .debug_line          PROGBITS     00000000 003a73 000223  0        0   0  1
[31] .note                NOTE         00000000 003c96 0000a0  0        0   0  1
[32] .shstrtab            STRTAB       00000000 003d36 00012e  0        0   0  1
[33] .symtab              NOBITS       00000000 003e64 0005a0 16       34  68  4
[34] .strtab              NOBITS       00000000 003e64 000235  0        0   0  1

EOF

# A random 64bit testfile
testfiles testfile69.so
# Explicitly keep .strtab (but not .symtab, so .strtab will be in both). 64bit
echo strip --keep-section=.strtab testfile69.so
testrun ${abs_top_builddir}/src/strip --keep-section=.strtab -o testfile.elf -f testfile.debug testfile69.so
echo elflint testfile.elf
testrun ${abs_top_builddir}/src/elflint --gnu testfile.elf
echo elflint testfile.debug
testrun ${abs_top_builddir}/src/elflint --gnu -d testfile.debug
echo readelf testfile.elf
testrun_compare ${abs_top_builddir}/src/readelf -S testfile.elf <<\EOF
There are 27 section headers, starting at offset 0xad8:

Section Headers:
[Nr] Name                 Type         Addr             Off      Size     ES Flags Lk Inf Al
[ 0]                      NULL         0000000000000000 00000000 00000000  0        0   0  0
[ 1] .note.gnu.build-id   NOTE         0000000000000190 00000190 00000024  0 A      0   0  4
[ 2] .gnu.hash            GNU_HASH     00000000000001b8 000001b8 0000003c  0 A      3   0  8
[ 3] .dynsym              DYNSYM       00000000000001f8 000001f8 00000108 24 A      4   2  8
[ 4] .dynstr              STRTAB       0000000000000300 00000300 00000077  0 A      0   0  1
[ 5] .gnu.version         GNU_versym   0000000000000378 00000378 00000016  2 A      3   0  2
[ 6] .gnu.version_r       GNU_verneed  0000000000000390 00000390 00000020  0 A      4   1  8
[ 7] .rela.dyn            RELA         00000000000003b0 000003b0 00000060 24 A      3   0  8
[ 8] .rela.plt            RELA         0000000000000410 00000410 00000018 24 A      3  10  8
[ 9] .init                PROGBITS     0000000000000428 00000428 00000018  0 AX     0   0  4
[10] .plt                 PROGBITS     0000000000000440 00000440 00000020 16 AX     0   0 16
[11] .text                PROGBITS     0000000000000460 00000460 00000128  0 AX     0   0 16
[12] .fini                PROGBITS     0000000000000588 00000588 0000000e  0 AX     0   0  4
[13] .eh_frame_hdr        PROGBITS     0000000000000598 00000598 00000024  0 A      0   0  4
[14] .eh_frame            PROGBITS     00000000000005c0 000005c0 00000084  0 A      0   0  8
[15] .ctors               PROGBITS     0000000000200648 00000648 00000010  0 WA     0   0  8
[16] .dtors               PROGBITS     0000000000200658 00000658 00000010  0 WA     0   0  8
[17] .jcr                 PROGBITS     0000000000200668 00000668 00000008  0 WA     0   0  8
[18] .data.rel.ro         PROGBITS     0000000000200670 00000670 00000008  0 WA     0   0  8
[19] .dynamic             DYNAMIC      0000000000200678 00000678 00000180 16 WA     4   0  8
[20] .got                 PROGBITS     00000000002007f8 000007f8 00000018  8 WA     0   0  8
[21] .got.plt             PROGBITS     0000000000200810 00000810 00000020  8 WA     0   0  8
[22] .bss                 NOBITS       0000000000200830 00000830 00000010  0 WA     0   0  8
[23] .comment             PROGBITS     0000000000000000 00000830 0000002c  1 MS     0   0  1
[24] .strtab              STRTAB       0000000000000000 0000085c 00000175  0        0   0  1
[25] .gnu_debuglink       PROGBITS     0000000000000000 000009d4 00000014  0        0   0  4
[26] .shstrtab            STRTAB       0000000000000000 000009e8 000000ee  0        0   0  1

EOF
echo readelf testfile.debug
testrun_compare ${abs_top_builddir}/src/readelf -S testfile.debug <<\EOF
There are 27 section headers, starting at offset 0x918:

Section Headers:
[Nr] Name                 Type         Addr             Off      Size     ES Flags Lk Inf Al
[ 0]                      NULL         0000000000000000 00000000 00000000  0        0   0  0
[ 1] .note.gnu.build-id   NOTE         0000000000000190 00000190 00000024  0 A      0   0  4
[ 2] .gnu.hash            NOBITS       00000000000001b8 000001b8 0000003c  0 A      3   0  8
[ 3] .dynsym              NOBITS       00000000000001f8 000001b8 00000108 24 A      4   2  8
[ 4] .dynstr              NOBITS       0000000000000300 000001b8 00000077  0 A      0   0  1
[ 5] .gnu.version         NOBITS       0000000000000378 000001b8 00000016  2 A      3   0  2
[ 6] .gnu.version_r       NOBITS       0000000000000390 000001b8 00000020  0 A      4   1  8
[ 7] .rela.dyn            NOBITS       00000000000003b0 000001b8 00000060 24 A      3   0  8
[ 8] .rela.plt            NOBITS       0000000000000410 000001b8 00000018 24 A      3  10  8
[ 9] .init                NOBITS       0000000000000428 000001b8 00000018  0 AX     0   0  4
[10] .plt                 NOBITS       0000000000000440 000001c0 00000020 16 AX     0   0 16
[11] .text                NOBITS       0000000000000460 000001c0 00000128  0 AX     0   0 16
[12] .fini                NOBITS       0000000000000588 000001c0 0000000e  0 AX     0   0  4
[13] .eh_frame_hdr        NOBITS       0000000000000598 000001c0 00000024  0 A      0   0  4
[14] .eh_frame            NOBITS       00000000000005c0 000001c0 00000084  0 A      0   0  8
[15] .ctors               NOBITS       0000000000200648 000001c0 00000010  0 WA     0   0  8
[16] .dtors               NOBITS       0000000000200658 000001c0 00000010  0 WA     0   0  8
[17] .jcr                 NOBITS       0000000000200668 000001c0 00000008  0 WA     0   0  8
[18] .data.rel.ro         NOBITS       0000000000200670 000001c0 00000008  0 WA     0   0  8
[19] .dynamic             NOBITS       0000000000200678 000001c0 00000180 16 WA     4   0  8
[20] .got                 NOBITS       00000000002007f8 000001c0 00000018  8 WA     0   0  8
[21] .got.plt             NOBITS       0000000000200810 000001c0 00000020  8 WA     0   0  8
[22] .bss                 NOBITS       0000000000200830 000001c0 00000010  0 WA     0   0  8
[23] .comment             NOBITS       0000000000000000 000001c0 0000002c  1 MS     0   0  1
[24] .shstrtab            STRTAB       0000000000000000 000001c0 000000e7  0        0   0  1
[25] .symtab              SYMTAB       0000000000000000 000002a8 000004f8 24       26  44  8
[26] .strtab              STRTAB       0000000000000000 000007a0 00000175  0        0   0  1

EOF

# Explicitly keep .symtab (pulls in .strtab, so they will both be in elf). 64bit
# Use --remove-comment to make sure testfile.debug isn't empty.
echo strip --keep-section=.symtab --remove-comment testfile69.so
testrun ${abs_top_builddir}/src/strip --keep-section=.symtab --remove-comment -o testfile.elf -f testfile.debug testfile69.so
echo elflint testfile.elf
testrun ${abs_top_builddir}/src/elflint --gnu testfile.elf
echo elflint testfile.debug
testrun ${abs_top_builddir}/src/elflint --gnu -d testfile.debug
echo readelf testfile.elf
testrun_compare ${abs_top_builddir}/src/readelf -S testfile.elf <<\EOF
There are 27 section headers, starting at offset 0xf90:

Section Headers:
[Nr] Name                 Type         Addr             Off      Size     ES Flags Lk Inf Al
[ 0]                      NULL         0000000000000000 00000000 00000000  0        0   0  0
[ 1] .note.gnu.build-id   NOTE         0000000000000190 00000190 00000024  0 A      0   0  4
[ 2] .gnu.hash            GNU_HASH     00000000000001b8 000001b8 0000003c  0 A      3   0  8
[ 3] .dynsym              DYNSYM       00000000000001f8 000001f8 00000108 24 A      4   2  8
[ 4] .dynstr              STRTAB       0000000000000300 00000300 00000077  0 A      0   0  1
[ 5] .gnu.version         GNU_versym   0000000000000378 00000378 00000016  2 A      3   0  2
[ 6] .gnu.version_r       GNU_verneed  0000000000000390 00000390 00000020  0 A      4   1  8
[ 7] .rela.dyn            RELA         00000000000003b0 000003b0 00000060 24 A      3   0  8
[ 8] .rela.plt            RELA         0000000000000410 00000410 00000018 24 A      3  10  8
[ 9] .init                PROGBITS     0000000000000428 00000428 00000018  0 AX     0   0  4
[10] .plt                 PROGBITS     0000000000000440 00000440 00000020 16 AX     0   0 16
[11] .text                PROGBITS     0000000000000460 00000460 00000128  0 AX     0   0 16
[12] .fini                PROGBITS     0000000000000588 00000588 0000000e  0 AX     0   0  4
[13] .eh_frame_hdr        PROGBITS     0000000000000598 00000598 00000024  0 A      0   0  4
[14] .eh_frame            PROGBITS     00000000000005c0 000005c0 00000084  0 A      0   0  8
[15] .ctors               PROGBITS     0000000000200648 00000648 00000010  0 WA     0   0  8
[16] .dtors               PROGBITS     0000000000200658 00000658 00000010  0 WA     0   0  8
[17] .jcr                 PROGBITS     0000000000200668 00000668 00000008  0 WA     0   0  8
[18] .data.rel.ro         PROGBITS     0000000000200670 00000670 00000008  0 WA     0   0  8
[19] .dynamic             DYNAMIC      0000000000200678 00000678 00000180 16 WA     4   0  8
[20] .got                 PROGBITS     00000000002007f8 000007f8 00000018  8 WA     0   0  8
[21] .got.plt             PROGBITS     0000000000200810 00000810 00000020  8 WA     0   0  8
[22] .bss                 NOBITS       0000000000200830 00000830 00000010  0 WA     0   0  8
[23] .symtab              SYMTAB       0000000000000000 00000830 000004e0 24       24  43  8
[24] .strtab              STRTAB       0000000000000000 00000d10 00000175  0        0   0  1
[25] .gnu_debuglink       PROGBITS     0000000000000000 00000e88 00000014  0        0   0  4
[26] .shstrtab            STRTAB       0000000000000000 00000e9c 000000ed  0        0   0  1

EOF
echo readelf testfile.debug
testrun_compare ${abs_top_builddir}/src/readelf -S testfile.debug <<\EOF
There are 27 section headers, starting at offset 0x2d8:

Section Headers:
[Nr] Name                 Type         Addr             Off      Size     ES Flags Lk Inf Al
[ 0]                      NULL         0000000000000000 00000000 00000000  0        0   0  0
[ 1] .note.gnu.build-id   NOTE         0000000000000190 00000190 00000024  0 A      0   0  4
[ 2] .gnu.hash            NOBITS       00000000000001b8 000001b8 0000003c  0 A      3   0  8
[ 3] .dynsym              NOBITS       00000000000001f8 000001b8 00000108 24 A      4   2  8
[ 4] .dynstr              NOBITS       0000000000000300 000001b8 00000077  0 A      0   0  1
[ 5] .gnu.version         NOBITS       0000000000000378 000001b8 00000016  2 A      3   0  2
[ 6] .gnu.version_r       NOBITS       0000000000000390 000001b8 00000020  0 A      4   1  8
[ 7] .rela.dyn            NOBITS       00000000000003b0 000001b8 00000060 24 A      3   0  8
[ 8] .rela.plt            NOBITS       0000000000000410 000001b8 00000018 24 A      3  10  8
[ 9] .init                NOBITS       0000000000000428 000001b8 00000018  0 AX     0   0  4
[10] .plt                 NOBITS       0000000000000440 000001c0 00000020 16 AX     0   0 16
[11] .text                NOBITS       0000000000000460 000001c0 00000128  0 AX     0   0 16
[12] .fini                NOBITS       0000000000000588 000001c0 0000000e  0 AX     0   0  4
[13] .eh_frame_hdr        NOBITS       0000000000000598 000001c0 00000024  0 A      0   0  4
[14] .eh_frame            NOBITS       00000000000005c0 000001c0 00000084  0 A      0   0  8
[15] .ctors               NOBITS       0000000000200648 000001c0 00000010  0 WA     0   0  8
[16] .dtors               NOBITS       0000000000200658 000001c0 00000010  0 WA     0   0  8
[17] .jcr                 NOBITS       0000000000200668 000001c0 00000008  0 WA     0   0  8
[18] .data.rel.ro         NOBITS       0000000000200670 000001c0 00000008  0 WA     0   0  8
[19] .dynamic             NOBITS       0000000000200678 000001c0 00000180 16 WA     4   0  8
[20] .got                 NOBITS       00000000002007f8 000001c0 00000018  8 WA     0   0  8
[21] .got.plt             NOBITS       0000000000200810 000001c0 00000020  8 WA     0   0  8
[22] .bss                 NOBITS       0000000000200830 000001c0 00000010  0 WA     0   0  8
[23] .comment             PROGBITS     0000000000000000 000001c0 0000002c  1 MS     0   0  1
[24] .shstrtab            STRTAB       0000000000000000 000001ec 000000e7  0        0   0  1
[25] .symtab              NOBITS       0000000000000000 000002d8 000004f8 24       26  44  8
[26] .strtab              NOBITS       0000000000000000 000002d8 00000175  0        0   0  1

EOF

# Explicitly remove .symtab (but not .strtab, so it will be in both). 32bit
echo strip -g --remove-section=.symtab testfile
testrun ${abs_top_builddir}/src/strip -g --remove-section=.symtab -o testfile.elf -f testfile.debug testfile
echo elflint testfile.elf
testrun ${abs_top_builddir}/src/elflint --gnu testfile.elf
echo elflint testfile.debug
testrun ${abs_top_builddir}/src/elflint --gnu -d testfile.debug
echo readelf testfile.elf
testrun_compare ${abs_top_builddir}/src/readelf -S testfile.elf <<\EOF
There are 28 section headers, starting at offset 0xafc:

Section Headers:
[Nr] Name                 Type         Addr     Off    Size   ES Flags Lk Inf Al
[ 0]                      NULL         00000000 000000 000000  0        0   0  0
[ 1] .interp              PROGBITS     080480f4 0000f4 000013  0 A      0   0  1
[ 2] .note.ABI-tag        NOTE         08048108 000108 000020  0 A      0   0  4
[ 3] .hash                HASH         08048128 000128 000030  4 A      4   0  4
[ 4] .dynsym              DYNSYM       08048158 000158 000070 16 A      5   1  4
[ 5] .dynstr              STRTAB       080481c8 0001c8 00008e  0 A      0   0  1
[ 6] .gnu.version         GNU_versym   08048256 000256 00000e  2 A      4   0  2
[ 7] .gnu.version_r       GNU_verneed  08048264 000264 000030  0 A      5   1  4
[ 8] .rel.got             REL          08048294 000294 000008  8 A      4  19  4
[ 9] .rel.plt             REL          0804829c 00029c 000020  8 A      4  11  4
[10] .init                PROGBITS     080482bc 0002bc 000018  0 AX     0   0  4
[11] .plt                 PROGBITS     080482d4 0002d4 000050  4 AX     0   0  4
[12] .text                PROGBITS     08048330 000330 00018c  0 AX     0   0 16
[13] .fini                PROGBITS     080484bc 0004bc 00001e  0 AX     0   0  4
[14] .rodata              PROGBITS     080484dc 0004dc 000008  0 A      0   0  4
[15] .data                PROGBITS     080494e4 0004e4 000010  0 WA     0   0  4
[16] .eh_frame            PROGBITS     080494f4 0004f4 000004  0 WA     0   0  4
[17] .ctors               PROGBITS     080494f8 0004f8 000008  0 WA     0   0  4
[18] .dtors               PROGBITS     08049500 000500 000008  0 WA     0   0  4
[19] .got                 PROGBITS     08049508 000508 000020  4 WA     0   0  4
[20] .dynamic             DYNAMIC      08049528 000528 0000a0  8 WA     5   0  4
[21] .sbss                PROGBITS     080495c8 0005c8 000000  0 W      0   0  1
[22] .bss                 NOBITS       080495c8 0005c8 00001c  0 WA     0   0  4
[23] .comment             PROGBITS     00000000 0005c8 000170  0        0   0  1
[24] .note                NOTE         00000000 000738 0000a0  0        0   0  1
[25] .strtab              STRTAB       00000000 0007d8 000235  0        0   0  1
[26] .gnu_debuglink       PROGBITS     00000000 000a10 000014  0        0   0  4
[27] .shstrtab            STRTAB       00000000 000a24 0000d7  0        0   0  1

EOF
echo readelf testfile.debug
testrun_compare ${abs_top_builddir}/src/readelf -S testfile.debug <<\EOF
There are 35 section headers, starting at offset 0x463c:

Section Headers:
[Nr] Name                 Type         Addr     Off    Size   ES Flags Lk Inf Al
[ 0]                      NULL         00000000 000000 000000  0        0   0  0
[ 1] .interp              NOBITS       080480f4 0000f4 000013  0 A      0   0  1
[ 2] .note.ABI-tag        NOTE         08048108 0000f4 000020  0 A      0   0  4
[ 3] .hash                NOBITS       08048128 000114 000030  4 A      4   0  4
[ 4] .dynsym              NOBITS       08048158 000114 000070 16 A      5   1  4
[ 5] .dynstr              NOBITS       080481c8 000114 00008e  0 A      0   0  1
[ 6] .gnu.version         NOBITS       08048256 000114 00000e  2 A      4   0  2
[ 7] .gnu.version_r       NOBITS       08048264 000114 000030  0 A      5   1  4
[ 8] .rel.got             NOBITS       08048294 000114 000008  8 A      4  19  4
[ 9] .rel.plt             NOBITS       0804829c 000114 000020  8 A      4  11  4
[10] .init                NOBITS       080482bc 000114 000018  0 AX     0   0  4
[11] .plt                 NOBITS       080482d4 000114 000050  4 AX     0   0  4
[12] .text                NOBITS       08048330 000120 00018c  0 AX     0   0 16
[13] .fini                NOBITS       080484bc 000120 00001e  0 AX     0   0  4
[14] .rodata              NOBITS       080484dc 000120 000008  0 A      0   0  4
[15] .data                NOBITS       080494e4 000120 000010  0 WA     0   0  4
[16] .eh_frame            NOBITS       080494f4 000120 000004  0 WA     0   0  4
[17] .ctors               NOBITS       080494f8 000120 000008  0 WA     0   0  4
[18] .dtors               NOBITS       08049500 000120 000008  0 WA     0   0  4
[19] .got                 NOBITS       08049508 000120 000020  4 WA     0   0  4
[20] .dynamic             NOBITS       08049528 000120 0000a0  8 WA     5   0  4
[21] .sbss                NOBITS       080495c8 000120 000000  0 W      0   0  1
[22] .bss                 NOBITS       080495c8 000120 00001c  0 WA     0   0  4
[23] .stab                PROGBITS     00000000 000120 000720 12       24   0  4
[24] .stabstr             STRTAB       00000000 000840 001934  0        0   0  1
[25] .comment             NOBITS       00000000 002174 000170  0        0   0  1
[26] .debug_aranges       PROGBITS     00000000 002174 000060  0        0   0  1
[27] .debug_pubnames      PROGBITS     00000000 0021d4 000055  0        0   0  1
[28] .debug_info          PROGBITS     00000000 002229 001678  0        0   0  1
[29] .debug_abbrev        PROGBITS     00000000 0038a1 0001d2  0        0   0  1
[30] .debug_line          PROGBITS     00000000 003a73 000223  0        0   0  1
[31] .note                NOTE         00000000 003c96 0000a0  0        0   0  1
[32] .shstrtab            STRTAB       00000000 003d36 00012e  0        0   0  1
[33] .symtab              SYMTAB       00000000 003e64 0005a0 16       34  68  4
[34] .strtab              STRTAB       00000000 004404 000235  0        0   0  1

EOF

# Explicitly remove both .symtab and .strtab. Keep .stab and .stabstr 32bit
echo strip -g --remove-section=".s[yt][mr]tab" --keep-section=".stab*" testfile
testrun ${abs_top_builddir}/src/strip -g --remove-section=".s[yt][mr]tab" --keep-section=".stab*" -o testfile.elf -f testfile.debug testfile
echo elflint testfile.elf
testrun ${abs_top_builddir}/src/elflint --gnu testfile.elf
echo elflint testfile.debug
testrun ${abs_top_builddir}/src/elflint --gnu -d testfile.debug
echo readelf testfile.elf
testrun_compare ${abs_top_builddir}/src/readelf -S testfile.elf <<\EOF
There are 29 section headers, starting at offset 0x2920:

Section Headers:
[Nr] Name                 Type         Addr     Off    Size   ES Flags Lk Inf Al
[ 0]                      NULL         00000000 000000 000000  0        0   0  0
[ 1] .interp              PROGBITS     080480f4 0000f4 000013  0 A      0   0  1
[ 2] .note.ABI-tag        NOTE         08048108 000108 000020  0 A      0   0  4
[ 3] .hash                HASH         08048128 000128 000030  4 A      4   0  4
[ 4] .dynsym              DYNSYM       08048158 000158 000070 16 A      5   1  4
[ 5] .dynstr              STRTAB       080481c8 0001c8 00008e  0 A      0   0  1
[ 6] .gnu.version         GNU_versym   08048256 000256 00000e  2 A      4   0  2
[ 7] .gnu.version_r       GNU_verneed  08048264 000264 000030  0 A      5   1  4
[ 8] .rel.got             REL          08048294 000294 000008  8 A      4  19  4
[ 9] .rel.plt             REL          0804829c 00029c 000020  8 A      4  11  4
[10] .init                PROGBITS     080482bc 0002bc 000018  0 AX     0   0  4
[11] .plt                 PROGBITS     080482d4 0002d4 000050  4 AX     0   0  4
[12] .text                PROGBITS     08048330 000330 00018c  0 AX     0   0 16
[13] .fini                PROGBITS     080484bc 0004bc 00001e  0 AX     0   0  4
[14] .rodata              PROGBITS     080484dc 0004dc 000008  0 A      0   0  4
[15] .data                PROGBITS     080494e4 0004e4 000010  0 WA     0   0  4
[16] .eh_frame            PROGBITS     080494f4 0004f4 000004  0 WA     0   0  4
[17] .ctors               PROGBITS     080494f8 0004f8 000008  0 WA     0   0  4
[18] .dtors               PROGBITS     08049500 000500 000008  0 WA     0   0  4
[19] .got                 PROGBITS     08049508 000508 000020  4 WA     0   0  4
[20] .dynamic             DYNAMIC      08049528 000528 0000a0  8 WA     5   0  4
[21] .sbss                PROGBITS     080495c8 0005c8 000000  0 W      0   0  1
[22] .bss                 NOBITS       080495c8 0005c8 00001c  0 WA     0   0  4
[23] .stab                PROGBITS     00000000 0005c8 000720 12       24   0  4
[24] .stabstr             STRTAB       00000000 000ce8 001934  0        0   0  1
[25] .comment             PROGBITS     00000000 00261c 000170  0        0   0  1
[26] .note                NOTE         00000000 00278c 0000a0  0        0   0  1
[27] .gnu_debuglink       PROGBITS     00000000 00282c 000014  0        0   0  4
[28] .shstrtab            STRTAB       00000000 002840 0000de  0        0   0  1

EOF
echo readelf testfile.debug
testrun_compare ${abs_top_builddir}/src/readelf -S testfile.debug <<\EOF
There are 35 section headers, starting at offset 0x25e8:

Section Headers:
[Nr] Name                 Type         Addr     Off    Size   ES Flags Lk Inf Al
[ 0]                      NULL         00000000 000000 000000  0        0   0  0
[ 1] .interp              NOBITS       080480f4 0000f4 000013  0 A      0   0  1
[ 2] .note.ABI-tag        NOTE         08048108 0000f4 000020  0 A      0   0  4
[ 3] .hash                NOBITS       08048128 000114 000030  4 A      4   0  4
[ 4] .dynsym              NOBITS       08048158 000114 000070 16 A      5   1  4
[ 5] .dynstr              NOBITS       080481c8 000114 00008e  0 A      0   0  1
[ 6] .gnu.version         NOBITS       08048256 000114 00000e  2 A      4   0  2
[ 7] .gnu.version_r       NOBITS       08048264 000114 000030  0 A      5   1  4
[ 8] .rel.got             NOBITS       08048294 000114 000008  8 A      4  19  4
[ 9] .rel.plt             NOBITS       0804829c 000114 000020  8 A      4  11  4
[10] .init                NOBITS       080482bc 000114 000018  0 AX     0   0  4
[11] .plt                 NOBITS       080482d4 000114 000050  4 AX     0   0  4
[12] .text                NOBITS       08048330 000120 00018c  0 AX     0   0 16
[13] .fini                NOBITS       080484bc 000120 00001e  0 AX     0   0  4
[14] .rodata              NOBITS       080484dc 000120 000008  0 A      0   0  4
[15] .data                NOBITS       080494e4 000120 000010  0 WA     0   0  4
[16] .eh_frame            NOBITS       080494f4 000120 000004  0 WA     0   0  4
[17] .ctors               NOBITS       080494f8 000120 000008  0 WA     0   0  4
[18] .dtors               NOBITS       08049500 000120 000008  0 WA     0   0  4
[19] .got                 NOBITS       08049508 000120 000020  4 WA     0   0  4
[20] .dynamic             NOBITS       08049528 000120 0000a0  8 WA     5   0  4
[21] .sbss                NOBITS       080495c8 000120 000000  0 W      0   0  1
[22] .bss                 NOBITS       080495c8 000120 00001c  0 WA     0   0  4
[23] .stab                NOBITS       00000000 000120 000720 12       24   0  4
[24] .stabstr             NOBITS       00000000 000120 001934  0        0   0  1
[25] .comment             NOBITS       00000000 000120 000170  0        0   0  1
[26] .debug_aranges       PROGBITS     00000000 000120 000060  0        0   0  1
[27] .debug_pubnames      PROGBITS     00000000 000180 000055  0        0   0  1
[28] .debug_info          PROGBITS     00000000 0001d5 001678  0        0   0  1
[29] .debug_abbrev        PROGBITS     00000000 00184d 0001d2  0        0   0  1
[30] .debug_line          PROGBITS     00000000 001a1f 000223  0        0   0  1
[31] .note                NOTE         00000000 001c42 0000a0  0        0   0  1
[32] .shstrtab            STRTAB       00000000 001ce2 00012e  0        0   0  1
[33] .symtab              SYMTAB       00000000 001e10 0005a0 16       34  68  4
[34] .strtab              STRTAB       00000000 0023b0 000235  0        0   0  1

EOF

# Explicitly remove .symtab (but not .strtab, so it will be in both). 64bit
echo strip -g --remove-section=.symtab testfile69.so
testrun ${abs_top_builddir}/src/strip -g --remove-section=.symtab -o testfile.elf -f testfile.debug testfile69.so
echo elflint testfile.elf
testrun ${abs_top_builddir}/src/elflint --gnu testfile.elf
echo elflint testfile.debug
testrun ${abs_top_builddir}/src/elflint --gnu -d testfile.debug
echo readelf testfile.elf
testrun_compare ${abs_top_builddir}/src/readelf -S testfile.elf <<\EOF
There are 27 section headers, starting at offset 0xad8:

Section Headers:
[Nr] Name                 Type         Addr             Off      Size     ES Flags Lk Inf Al
[ 0]                      NULL         0000000000000000 00000000 00000000  0        0   0  0
[ 1] .note.gnu.build-id   NOTE         0000000000000190 00000190 00000024  0 A      0   0  4
[ 2] .gnu.hash            GNU_HASH     00000000000001b8 000001b8 0000003c  0 A      3   0  8
[ 3] .dynsym              DYNSYM       00000000000001f8 000001f8 00000108 24 A      4   2  8
[ 4] .dynstr              STRTAB       0000000000000300 00000300 00000077  0 A      0   0  1
[ 5] .gnu.version         GNU_versym   0000000000000378 00000378 00000016  2 A      3   0  2
[ 6] .gnu.version_r       GNU_verneed  0000000000000390 00000390 00000020  0 A      4   1  8
[ 7] .rela.dyn            RELA         00000000000003b0 000003b0 00000060 24 A      3   0  8
[ 8] .rela.plt            RELA         0000000000000410 00000410 00000018 24 A      3  10  8
[ 9] .init                PROGBITS     0000000000000428 00000428 00000018  0 AX     0   0  4
[10] .plt                 PROGBITS     0000000000000440 00000440 00000020 16 AX     0   0 16
[11] .text                PROGBITS     0000000000000460 00000460 00000128  0 AX     0   0 16
[12] .fini                PROGBITS     0000000000000588 00000588 0000000e  0 AX     0   0  4
[13] .eh_frame_hdr        PROGBITS     0000000000000598 00000598 00000024  0 A      0   0  4
[14] .eh_frame            PROGBITS     00000000000005c0 000005c0 00000084  0 A      0   0  8
[15] .ctors               PROGBITS     0000000000200648 00000648 00000010  0 WA     0   0  8
[16] .dtors               PROGBITS     0000000000200658 00000658 00000010  0 WA     0   0  8
[17] .jcr                 PROGBITS     0000000000200668 00000668 00000008  0 WA     0   0  8
[18] .data.rel.ro         PROGBITS     0000000000200670 00000670 00000008  0 WA     0   0  8
[19] .dynamic             DYNAMIC      0000000000200678 00000678 00000180 16 WA     4   0  8
[20] .got                 PROGBITS     00000000002007f8 000007f8 00000018  8 WA     0   0  8
[21] .got.plt             PROGBITS     0000000000200810 00000810 00000020  8 WA     0   0  8
[22] .bss                 NOBITS       0000000000200830 00000830 00000010  0 WA     0   0  8
[23] .comment             PROGBITS     0000000000000000 00000830 0000002c  1 MS     0   0  1
[24] .strtab              STRTAB       0000000000000000 0000085c 00000175  0        0   0  1
[25] .gnu_debuglink       PROGBITS     0000000000000000 000009d4 00000014  0        0   0  4
[26] .shstrtab            STRTAB       0000000000000000 000009e8 000000ee  0        0   0  1

EOF
echo readelf testfile.debug
testrun_compare ${abs_top_builddir}/src/readelf -S testfile.debug <<\EOF
There are 27 section headers, starting at offset 0x918:

Section Headers:
[Nr] Name                 Type         Addr             Off      Size     ES Flags Lk Inf Al
[ 0]                      NULL         0000000000000000 00000000 00000000  0        0   0  0
[ 1] .note.gnu.build-id   NOTE         0000000000000190 00000190 00000024  0 A      0   0  4
[ 2] .gnu.hash            NOBITS       00000000000001b8 000001b8 0000003c  0 A      3   0  8
[ 3] .dynsym              NOBITS       00000000000001f8 000001b8 00000108 24 A      4   2  8
[ 4] .dynstr              NOBITS       0000000000000300 000001b8 00000077  0 A      0   0  1
[ 5] .gnu.version         NOBITS       0000000000000378 000001b8 00000016  2 A      3   0  2
[ 6] .gnu.version_r       NOBITS       0000000000000390 000001b8 00000020  0 A      4   1  8
[ 7] .rela.dyn            NOBITS       00000000000003b0 000001b8 00000060 24 A      3   0  8
[ 8] .rela.plt            NOBITS       0000000000000410 000001b8 00000018 24 A      3  10  8
[ 9] .init                NOBITS       0000000000000428 000001b8 00000018  0 AX     0   0  4
[10] .plt                 NOBITS       0000000000000440 000001c0 00000020 16 AX     0   0 16
[11] .text                NOBITS       0000000000000460 000001c0 00000128  0 AX     0   0 16
[12] .fini                NOBITS       0000000000000588 000001c0 0000000e  0 AX     0   0  4
[13] .eh_frame_hdr        NOBITS       0000000000000598 000001c0 00000024  0 A      0   0  4
[14] .eh_frame            NOBITS       00000000000005c0 000001c0 00000084  0 A      0   0  8
[15] .ctors               NOBITS       0000000000200648 000001c0 00000010  0 WA     0   0  8
[16] .dtors               NOBITS       0000000000200658 000001c0 00000010  0 WA     0   0  8
[17] .jcr                 NOBITS       0000000000200668 000001c0 00000008  0 WA     0   0  8
[18] .data.rel.ro         NOBITS       0000000000200670 000001c0 00000008  0 WA     0   0  8
[19] .dynamic             NOBITS       0000000000200678 000001c0 00000180 16 WA     4   0  8
[20] .got                 NOBITS       00000000002007f8 000001c0 00000018  8 WA     0   0  8
[21] .got.plt             NOBITS       0000000000200810 000001c0 00000020  8 WA     0   0  8
[22] .bss                 NOBITS       0000000000200830 000001c0 00000010  0 WA     0   0  8
[23] .comment             NOBITS       0000000000000000 000001c0 0000002c  1 MS     0   0  1
[24] .shstrtab            STRTAB       0000000000000000 000001c0 000000e7  0        0   0  1
[25] .symtab              SYMTAB       0000000000000000 000002a8 000004f8 24       26  44  8
[26] .strtab              STRTAB       0000000000000000 000007a0 00000175  0        0   0  1

EOF

# Explicitly remove both .symtab and .strtab. Keep .comment section. 64bit
echo strip -g --remove-section=".s[yt][mr]tab" --keep-section=.comment testfile69.so
testrun ${abs_top_builddir}/src/strip -g --remove-section=".s[yt][mr]tab" --keep-section=.comment -o testfile.elf -f testfile.debug testfile69.so
echo elflint testfile.elf
testrun ${abs_top_builddir}/src/elflint --gnu testfile.elf
echo elflint testfile.debug
testrun ${abs_top_builddir}/src/elflint --gnu -d testfile.debug
echo readelf testfile.elf
testrun_compare ${abs_top_builddir}/src/readelf -S testfile.elf <<\EOF
There are 26 section headers, starting at offset 0x958:

Section Headers:
[Nr] Name                 Type         Addr             Off      Size     ES Flags Lk Inf Al
[ 0]                      NULL         0000000000000000 00000000 00000000  0        0   0  0
[ 1] .note.gnu.build-id   NOTE         0000000000000190 00000190 00000024  0 A      0   0  4
[ 2] .gnu.hash            GNU_HASH     00000000000001b8 000001b8 0000003c  0 A      3   0  8
[ 3] .dynsym              DYNSYM       00000000000001f8 000001f8 00000108 24 A      4   2  8
[ 4] .dynstr              STRTAB       0000000000000300 00000300 00000077  0 A      0   0  1
[ 5] .gnu.version         GNU_versym   0000000000000378 00000378 00000016  2 A      3   0  2
[ 6] .gnu.version_r       GNU_verneed  0000000000000390 00000390 00000020  0 A      4   1  8
[ 7] .rela.dyn            RELA         00000000000003b0 000003b0 00000060 24 A      3   0  8
[ 8] .rela.plt            RELA         0000000000000410 00000410 00000018 24 A      3  10  8
[ 9] .init                PROGBITS     0000000000000428 00000428 00000018  0 AX     0   0  4
[10] .plt                 PROGBITS     0000000000000440 00000440 00000020 16 AX     0   0 16
[11] .text                PROGBITS     0000000000000460 00000460 00000128  0 AX     0   0 16
[12] .fini                PROGBITS     0000000000000588 00000588 0000000e  0 AX     0   0  4
[13] .eh_frame_hdr        PROGBITS     0000000000000598 00000598 00000024  0 A      0   0  4
[14] .eh_frame            PROGBITS     00000000000005c0 000005c0 00000084  0 A      0   0  8
[15] .ctors               PROGBITS     0000000000200648 00000648 00000010  0 WA     0   0  8
[16] .dtors               PROGBITS     0000000000200658 00000658 00000010  0 WA     0   0  8
[17] .jcr                 PROGBITS     0000000000200668 00000668 00000008  0 WA     0   0  8
[18] .data.rel.ro         PROGBITS     0000000000200670 00000670 00000008  0 WA     0   0  8
[19] .dynamic             DYNAMIC      0000000000200678 00000678 00000180 16 WA     4   0  8
[20] .got                 PROGBITS     00000000002007f8 000007f8 00000018  8 WA     0   0  8
[21] .got.plt             PROGBITS     0000000000200810 00000810 00000020  8 WA     0   0  8
[22] .bss                 NOBITS       0000000000200830 00000830 00000010  0 WA     0   0  8
[23] .comment             PROGBITS     0000000000000000 00000830 0000002c  1 MS     0   0  1
[24] .gnu_debuglink       PROGBITS     0000000000000000 0000085c 00000014  0        0   0  4
[25] .shstrtab            STRTAB       0000000000000000 00000870 000000e6  0        0   0  1

EOF
echo readelf testfile.debug
testrun_compare ${abs_top_builddir}/src/readelf -S testfile.debug <<\EOF
There are 27 section headers, starting at offset 0x918:

Section Headers:
[Nr] Name                 Type         Addr             Off      Size     ES Flags Lk Inf Al
[ 0]                      NULL         0000000000000000 00000000 00000000  0        0   0  0
[ 1] .note.gnu.build-id   NOTE         0000000000000190 00000190 00000024  0 A      0   0  4
[ 2] .gnu.hash            NOBITS       00000000000001b8 000001b8 0000003c  0 A      3   0  8
[ 3] .dynsym              NOBITS       00000000000001f8 000001b8 00000108 24 A      4   2  8
[ 4] .dynstr              NOBITS       0000000000000300 000001b8 00000077  0 A      0   0  1
[ 5] .gnu.version         NOBITS       0000000000000378 000001b8 00000016  2 A      3   0  2
[ 6] .gnu.version_r       NOBITS       0000000000000390 000001b8 00000020  0 A      4   1  8
[ 7] .rela.dyn            NOBITS       00000000000003b0 000001b8 00000060 24 A      3   0  8
[ 8] .rela.plt            NOBITS       0000000000000410 000001b8 00000018 24 A      3  10  8
[ 9] .init                NOBITS       0000000000000428 000001b8 00000018  0 AX     0   0  4
[10] .plt                 NOBITS       0000000000000440 000001c0 00000020 16 AX     0   0 16
[11] .text                NOBITS       0000000000000460 000001c0 00000128  0 AX     0   0 16
[12] .fini                NOBITS       0000000000000588 000001c0 0000000e  0 AX     0   0  4
[13] .eh_frame_hdr        NOBITS       0000000000000598 000001c0 00000024  0 A      0   0  4
[14] .eh_frame            NOBITS       00000000000005c0 000001c0 00000084  0 A      0   0  8
[15] .ctors               NOBITS       0000000000200648 000001c0 00000010  0 WA     0   0  8
[16] .dtors               NOBITS       0000000000200658 000001c0 00000010  0 WA     0   0  8
[17] .jcr                 NOBITS       0000000000200668 000001c0 00000008  0 WA     0   0  8
[18] .data.rel.ro         NOBITS       0000000000200670 000001c0 00000008  0 WA     0   0  8
[19] .dynamic             NOBITS       0000000000200678 000001c0 00000180 16 WA     4   0  8
[20] .got                 NOBITS       00000000002007f8 000001c0 00000018  8 WA     0   0  8
[21] .got.plt             NOBITS       0000000000200810 000001c0 00000020  8 WA     0   0  8
[22] .bss                 NOBITS       0000000000200830 000001c0 00000010  0 WA     0   0  8
[23] .comment             NOBITS       0000000000000000 000001c0 0000002c  1 MS     0   0  1
[24] .shstrtab            STRTAB       0000000000000000 000001c0 000000e7  0        0   0  1
[25] .symtab              SYMTAB       0000000000000000 000002a8 000004f8 24       26  44  8
[26] .strtab              STRTAB       0000000000000000 000007a0 00000175  0        0   0  1

EOF

exit 0
