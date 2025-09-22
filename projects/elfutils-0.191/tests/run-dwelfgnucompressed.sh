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

# = funcs.s =
# .globl testfunc
# testfunc:
# 	nop
# 	ret
# .type testfunc, @function
# .size testfunc, .-testfunc
#
# .globl testfunc2
# testfunc2:
# 	call testfunc
# 	nop
# 	nop
# 	ret
# .type testfunc2, @function
# .size testfunc2, .-testfunc2
#
# .globl functest3
# functest3:
# 	jmp local
# 	nop
# 	nop
# local:
# 	call testfunc2
# 	ret
# .type functest3, @function
# .size functest3, .-functest3

# = start.s =
# .global _start
# _start:
# 	call functest3
# 	nop
# 	nop
# 	nop
# 	nop
# 	nop
# 	nop
# 	nop
# 	nop
# 	nop
# 	nop
# 	nop
# 	nop
# 	nop
# 	nop
# 	nop
# 	nop
# 	ret
# .type _start, @function
# .size _start, .-_start

# gas --compress-debug-sections=zlib-gnu -32 -g -o start.o start.s
# gas --compress-debug-sections=zlib-gnu -32 -g -o funcs.o funcs.s
# ld --compress-debug-sections=zlib-gnu -melf_i386 -g -o zgnu32 funcs.o start.o

# gas --compress-debug-sections=zlib-gnu -64 -g -o start.o start.s
# gas --compress-debug-sections=zlib-gnu -64 -g -o funcs.o funcs.s
# ld --compress-debug-sections=zlib-gnu -g -o zgnu32 funcs.o start.o

testfiles testfile-zgnu64
testrun_compare ${abs_top_builddir}/tests/dwelfgnucompressed testfile-zgnu64 <<\EOF
section 2: GNU Compressed size: 60
section 3: GNU Compressed size: aa
section 5: GNU Compressed size: 8d
EOF

testfiles testfile-zgnu64be
testrun_compare ${abs_top_builddir}/tests/dwelfgnucompressed testfile-zgnu64be <<\EOF
section 3: GNU Compressed size: 60
section 4: GNU Compressed size: 7e
section 6: GNU Compressed size: 8d
EOF

testfiles testfile-zgnu32
testrun_compare ${abs_top_builddir}/tests/dwelfgnucompressed testfile-zgnu32 <<\EOF
section 2: GNU Compressed size: 40
section 3: GNU Compressed size: 9a
section 5: GNU Compressed size: 85
EOF

testfiles testfile-zgnu32be
testrun_compare ${abs_top_builddir}/tests/dwelfgnucompressed testfile-zgnu32be <<\EOF
section 3: GNU Compressed size: 40
section 4: GNU Compressed size: 6e
section 6: GNU Compressed size: 85
EOF

exit 0
