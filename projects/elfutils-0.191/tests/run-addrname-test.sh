#! /bin/sh
# Copyright (C) 2007, 2008 Red Hat, Inc.
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

testfiles testfile34 testfile38 testfile41 testfile49

testrun_compare ${abs_top_builddir}/src/addr2line -f -e testfile34 \
				 0x08048074 0x08048075 0x08048076 \
				 0x08049078 0x08048080 0x08049080 <<\EOF
foo
??:0
bar
??:0
_etext
??:0
data1
??:0
??
??:0
_end
??:0
EOF

testrun_compare ${abs_top_builddir}/src/addr2line -S -e testfile38 0x02 0x10a 0x211 0x31a <<\EOF
t1_global_outer+0x2
??:0
t2_global_symbol+0x2
??:0
t3_global_after_0+0x1
??:0
(.text)+0x31a
??:0
EOF

testrun_compare ${abs_top_builddir}/src/addr2line -S -e testfile41 0x1 0x104 <<\EOF
small_global_at_large_global+0x1
??:0
small_global_first_at_large_global+0x1
??:0
EOF

testfiles testfile12 testfile14
tempfiles testmaps

cat > testmaps <<EOF
00400000-00401000 r-xp 00000000 fd:01 4006812                            `pwd`/testfile14
00500000-00501000 rw-p 00000000 fd:01 4006812                            `pwd`/testfile14
01000000-01001000 r-xp 00000000 fd:01 1234567				 `pwd`/testfile12
01100000-01011000 rw-p 00000000 fd:01 1234567				 `pwd`/testfile12
2aaaaaaab000-2aaaaaaad000 rw-p 2aaaaaaab000 00:00 0 
2aaaaaae2000-2aaaaaae3000 rw-p 2aaaaaae2000 00:00 0 
7fff61068000-7fff6107d000 rw-p 7ffffffea000 00:00 0                      [stack]
7fff611fe000-7fff61200000 r-xp 7fff611fe000 00:00 0                      [vdso]
ffffffffff600000-ffffffffff601000 r-xp 00000000 00:00 0                  [vsyscall]
EOF

testrun_compare ${abs_top_builddir}/src/addr2line -S -M testmaps 0x40047c 0x10009db <<\EOF
caller+0x14
/home/drepper/local/elfutils-build/20050425/v.c:11
foo+0xb
/home/drepper/local/elfutils-build/20030710/u.c:5
EOF

#	.section .text
#	nop #0
#sizeless_foo:
#	nop #1
#	nop #2
#sized_bar:
#	nop #3
#	nop #4
#sizeless_baz:
#	nop #5
#	nop #6
#	.size sized_bar, . - sized_bar
#	nop #7
#	nop #8
#sizeless_x:
#	nop #9
#	.org 0x100
#	nop #0
#	.globl global_outer
#global_outer:
#	nop #1
#	nop #2
#	.globl global_in_global
#global_in_global:
#	nop #3
#	nop #4
#	.size global_in_global, . - global_in_global
#local_in_global:
#	nop #5 
#	nop #6 
#	.size local_in_global, . - local_in_global
#	nop #7
#	nop #8
#.Lsizeless1:
#	nop #9
#	nop #10
#	.size global_outer, . - global_outer
#	nop #11
#	.org 0x200
#	nop #0
#local_outer:
#	nop #1
#	nop #2
#	.globl global_in_local
#global_in_local:
#	nop #3
#	nop #4
#	.size global_in_local, . - global_in_local
#local_in_local:
#	nop #5 
#	nop #6 
#	.size local_in_local, . - local_in_local
#	nop #7
#	nop #8
#.Lsizeless2:
#	nop #9
#	nop #10
#	.size local_outer, . - local_outer
#	nop #11
testrun_compare ${abs_top_builddir}/src/addr2line -S -e testfile49 \
    		0 1 2 3 4 5 6 7 8 9 \
		0x100 0x101 0x102 0x103 0x104 0x105 \
		0x106 0x107 0x108 0x109 0x10a 0x10b \
		0x200 0x201 0x202 0x203 0x204 0x205 \
		0x206 0x207 0x208 0x209 0x20a 0x20b <<\EOF
(.text)+0
??:0
sizeless_foo
??:0
sizeless_foo+0x1
??:0
sized_bar
??:0
sized_bar+0x1
??:0
sized_bar+0x2
??:0
sized_bar+0x3
??:0
(.text)+0x7
??:0
(.text)+0x8
??:0
sizeless_x
??:0
sizeless_x+0xf7
??:0
global_outer
??:0
global_outer+0x1
??:0
global_in_global
??:0
global_in_global+0x1
??:0
global_outer+0x4
??:0
global_outer+0x5
??:0
global_outer+0x6
??:0
global_outer+0x7
??:0
global_outer+0x8
??:0
global_outer+0x9
??:0
(.text)+0x10b
??:0
(.text)+0x200
??:0
local_outer
??:0
local_outer+0x1
??:0
global_in_local
??:0
global_in_local+0x1
??:0
local_in_local
??:0
local_in_local+0x1
??:0
local_outer+0x6
??:0
local_outer+0x7
??:0
local_outer+0x8
??:0
local_outer+0x9
??:0
(.text)+0x20b
??:0
EOF

#	.macro global label size
#\label:	.globl \label
#	.size \label, \size
#	.endm
#	.macro weak label size
#\label:	.weak \label
#	.size \label, \size
#	.endm
#	.macro local label size
#\label:	.size \label, \size
#	.endm
#	.macro offset val
#	.ifne (. - _start) - \val
#	.err
#	.endif
#	.byte \val
#	.endm
#
#_start:
#	offset 0
#
#	local glocal, 1
#	weak gweak, 1
#	global gglobal1, 2
#	global gglobal2, 1
#	global gglobal3, 1
#	offset 1
#	/* Symbols end here.  */
#	offset 2
#	/* gglobal1 ends here.  */
#	offset 3
#
#	local g0local, 0
#	weak g0weak, 0
#	global g0global1, 0
#	global g0global2, 0
#	offset 4
#
#	local wlocal, 1
#	weak wweak1, 2
#	weak wweak2, 1
#	weak wweak3, 1
#	offset 5
#	/* Symbols end here.  */
#	offset 6
#	/* wweak1 ends here.  */
#	offset 7
#
#	local w0local, 0
#	weak w0weak1, 0
#	weak w0weak2, 0
#	offset 8
#
#	local llocal1, 2
#	local llocal2, 1
#	local llocal3, 1
#	offset 9
#	/* Symbols end here.  */
#	offset 10
#	/* llocal1 ends here.  */
#	offset 11
#
#	local l0local1, 0
#	local l0local2, 0
#	offset 12
testfiles testfile64
testrun_compare ${abs_top_builddir}/src/addr2line -S -e testfile64 1 4 5 8 9 c <<\EOF
gglobal2
??:0
g0global2
??:0
wweak2
??:0
w0weak2
??:0
llocal2
??:0
l0local2
??:0
EOF

testfiles testfile65
testrun_compare ${abs_top_builddir}/src/addr2line -S --core=testfile65 0x7fff94bffa30 <<\EOF
__vdso_time
??:0
EOF

#	.section	".text"
#	.globl _start
#	.section	".opd","aw"
#_start:	.quad	.L._start,.TOC.@tocbase
#	.previous
#	.type	_start, @function
#.L._start:
#	.byte	0x7d, 0x82, 0x10, 0x08
#	.size	_start,.-.L._start
testfiles testfile66 testfile66.core
testrun_compare ${abs_top_builddir}/src/addr2line -x -e testfile66 _start 0x2d8 0x2db 0x2dc 0x103d0 0x103d3 0x103d4<<EOF
_start (.text)
??:0
_start (.text)
??:0
_start+0x3 (.text)
??:0
()+0x2dc
??:0
_start (.opd)
??:0
_start+0x3 (.opd)
??:0
()+0x103d4
??:0
EOF
testrun_compare ${abs_top_builddir}/src/addr2line -x -e testfile66 --core=testfile66.core _start 0x461b02d8 0x461c03d0<<\EOF
_start (.text)
??:0
_start (.text)
??:0
_start (.opd)
??:0
EOF

testfiles testfile69.core testfile69.so
testrun_compare ${abs_top_builddir}/src/addr2line --core=./testfile69.core -S 0x7f0bc6a33535 0x7f0bc6a33546 <<\EOF
libstatic+0x9
??:0
libglobal+0x9
??:0
EOF

testfiles testfile70.exec testfile70.core
testrun_compare ${abs_top_builddir}/src/addr2line -S -e testfile70.exec --core=testfile70.core 0x7ff2cfe9b6b5 <<\EOF
main+0x9
??:0
EOF
testrun_compare ${abs_top_builddir}/src/addr2line -S --core=testfile70.core -e testfile70.exec 0x7ff2cfe9b6b5 <<\EOF
main+0x9
??:0
EOF

testfiles test-core-lib.so test-core.core test-core.exec
testrun_compare ${abs_top_builddir}/src/addr2line -S -e test-core.exec --core=test-core.core 0x7f67f2aaf619 <<\EOF
libfunc+0x9
??:0
EOF

exit 0
