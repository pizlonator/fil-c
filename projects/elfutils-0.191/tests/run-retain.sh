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

# - retian.s
#   .section        .data.retain,"R" 
#   .word   1
#
# as -o testfile-retain.o retain.s

testfiles testfile-retain.o

testrun_compare ${abs_top_builddir}/src/readelf -S testfile-retain.o << EOF
There are 9 section headers, starting at offset 0x158:

Section Headers:
[Nr] Name                 Type         Addr             Off      Size     ES Flags Lk Inf Al
[ 0]                      NULL         0000000000000000 00000000 00000000  0        0   0  0
[ 1] .text                PROGBITS     0000000000000000 00000040 00000000  0 AX     0   0  1
[ 2] .data                PROGBITS     0000000000000000 00000040 00000000  0 WA     0   0  1
[ 3] .bss                 NOBITS       0000000000000000 00000040 00000000  0 WA     0   0  1
[ 4] .data.retain         PROGBITS     0000000000000000 00000040 00000002  0 WAR    0   0  1
[ 5] .note.gnu.property   NOTE         0000000000000000 00000048 00000030  0 A      0   0  8
[ 6] .symtab              SYMTAB       0000000000000000 00000078 00000090 24        7   6  8
[ 7] .strtab              STRTAB       0000000000000000 00000108 00000001  0        0   0  1
[ 8] .shstrtab            STRTAB       0000000000000000 00000109 0000004c  0        0   0  1

EOF

testrun ${abs_top_builddir}/src/elflint --gnu testfile-retain.o
