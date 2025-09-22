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

# void *SYM1;
# void *SYM2 = 0;
# extern void *SYM3;
# static void *SYM4;
#
# void *SYM6 = &SYM3;
# static void *SYM7 = &SYM3;
#
# void *SYM8 __attribute__((__weak__));
#
# void FUN1 (void) { }
# static void FUN2 (void) { }
# extern void FUN3 (void);
#
# void *FREF = FUN3;
#
# void __attribute__((__weak__)) FUN4 (void) { };
#
# int NUM0 = 0; __asm__(".type NUM0,%gnu_unique_object");
# int __thread NUM1 = 1;
#
# gcc -m64 -c syms.c -o testfilesyms64
# gcc -m32 -c syms.c -o testfilesyms32

testfiles testfilesyms32 testfilesyms64

testrun_compare ${abs_top_builddir}/src/nm --format=bsd testfilesyms32 <<\EOF
00000008 D FREF
00000000 T FUN1
00000010 t FUN2
         U FUN3
00000020 W FUN4
00000008 u NUM0
00000000 D NUM1
00000004 C SYM1
00000000 B SYM2
         U SYM3
0000000c b SYM4
00000000 D SYM6
00000004 d SYM7
00000004 V SYM8
         U _GLOBAL_OFFSET_TABLE_
00000000 T __x86.get_pc_thunk.ax
EOF

testrun_compare ${abs_top_builddir}/src/nm --format=posix testfilesyms32 <<\EOF
FREF D 00000008 00000004
FUN1 T 00000000 00000010
FUN2 t 00000010 00000010
FUN3 U
FUN4 W 00000020 00000010
NUM0 u 00000008 00000004
NUM1 D 00000000 00000004
SYM1 C 00000004 00000004
SYM2 B 00000000 00000004
SYM3 U
SYM4 b 0000000c 00000004
SYM6 D 00000000 00000004
SYM7 d 00000004 00000004
SYM8 V 00000004 00000004
_GLOBAL_OFFSET_TABLE_ U
__x86.get_pc_thunk.ax T 00000000 00000000
EOF

testrun_compare ${abs_top_builddir}/src/nm --format=sysv testfilesyms32 <<\EOF


Symbols from testfilesyms32:

Name                  Value    Class  Type     Size     Line Section

FREF                 |00000008|GLOBAL|OBJECT  |00000004|    |.data.rel
FUN1                 |00000000|GLOBAL|FUNC    |00000010|    |.text
FUN2                 |00000010|LOCAL |FUNC    |00000010|    |.text
FUN3                 |        |GLOBAL|NOTYPE  |        |    |UNDEF
FUN4                 |00000020|WEAK  |FUNC    |00000010|    |.text
NUM0                 |00000008|UNIQUE|OBJECT  |00000004|    |.bss
NUM1                 |00000000|GLOBAL|TLS     |00000004|    |.tdata
SYM1                 |00000004|GLOBAL|OBJECT  |00000004|    |COMMON
SYM2                 |00000000|GLOBAL|OBJECT  |00000004|    |.bss
SYM3                 |        |GLOBAL|NOTYPE  |        |    |UNDEF
SYM4                 |0000000c|LOCAL |OBJECT  |00000004|    |.bss
SYM6                 |00000000|GLOBAL|OBJECT  |00000004|    |.data.rel
SYM7                 |00000004|LOCAL |OBJECT  |00000004|    |.data.rel
SYM8                 |00000004|WEAK  |OBJECT  |00000004|    |.bss
_GLOBAL_OFFSET_TABLE_|        |GLOBAL|NOTYPE  |        |    |UNDEF
__x86.get_pc_thunk.ax|00000000|GLOBAL|FUNC    |00000000|    |.text.__x86.get_pc_thunk.ax
EOF

testrun_compare ${abs_top_builddir}/src/nm --format=bsd testfilesyms64 <<\EOF
0000000000000010 D FREF
0000000000000000 T FUN1
0000000000000007 t FUN2
                 U FUN3
000000000000000e W FUN4
0000000000000010 u NUM0
0000000000000000 D NUM1
0000000000000008 C SYM1
0000000000000000 B SYM2
                 U SYM3
0000000000000018 b SYM4
0000000000000000 D SYM6
0000000000000008 d SYM7
0000000000000008 V SYM8
EOF

testrun_compare ${abs_top_builddir}/src/nm --format=posix testfilesyms64 <<\EOF
FREF D 0000000000000010 0000000000000008
FUN1 T 0000000000000000 0000000000000007
FUN2 t 0000000000000007 0000000000000007
FUN3 U
FUN4 W 000000000000000e 0000000000000007
NUM0 u 0000000000000010 0000000000000004
NUM1 D 0000000000000000 0000000000000004
SYM1 C 0000000000000008 0000000000000008
SYM2 B 0000000000000000 0000000000000008
SYM3 U
SYM4 b 0000000000000018 0000000000000008
SYM6 D 0000000000000000 0000000000000008
SYM7 d 0000000000000008 0000000000000008
SYM8 V 0000000000000008 0000000000000008
EOF

testrun_compare ${abs_top_builddir}/src/nm --format=sysv testfilesyms64 <<\EOF


Symbols from testfilesyms64:

Name   Value            Class  Type     Size             Line Section

FREF  |0000000000000010|GLOBAL|OBJECT  |0000000000000008|    |.data.rel
FUN1  |0000000000000000|GLOBAL|FUNC    |0000000000000007|    |.text
FUN2  |0000000000000007|LOCAL |FUNC    |0000000000000007|    |.text
FUN3  |                |GLOBAL|NOTYPE  |                |    |UNDEF
FUN4  |000000000000000e|WEAK  |FUNC    |0000000000000007|    |.text
NUM0  |0000000000000010|UNIQUE|OBJECT  |0000000000000004|    |.bss
NUM1  |0000000000000000|GLOBAL|TLS     |0000000000000004|    |.tdata
SYM1  |0000000000000008|GLOBAL|OBJECT  |0000000000000008|    |COMMON
SYM2  |0000000000000000|GLOBAL|OBJECT  |0000000000000008|    |.bss
SYM3  |                |GLOBAL|NOTYPE  |                |    |UNDEF
SYM4  |0000000000000018|LOCAL |OBJECT  |0000000000000008|    |.bss
SYM6  |0000000000000000|GLOBAL|OBJECT  |0000000000000008|    |.data.rel
SYM7  |0000000000000008|LOCAL |OBJECT  |0000000000000008|    |.data.rel
SYM8  |0000000000000008|WEAK  |OBJECT  |0000000000000008|    |.bss
EOF

exit 0
