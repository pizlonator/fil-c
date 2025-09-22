#! /bin/sh
# Test for dwarf_getcfi.
# Copyright (C) 2018 Red Hat, Inc.
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

# Test files come from run-addrcfi with all sections stripped except
# the .debug_frame.
# for i in <testfiles>
#   eu-strip -f $i-debugframe $i
#   eu-strip -g --remove-comment --keep-section=.debug_frame $i-debugframe
# done
testfiles testfile11-debugframe testfile12-debugframe
testfiles testfileaarch64-debugframe
testfiles testfilearm-debugframe
testfiles testfileppc32-debugframe
testfiles testfileppc64-debugframe

testfiles testfile11-debugframe
testrun_compare ${abs_builddir}/dwarfcfi testfile11-debugframe 0x080489b8 <<\EOF
0x80489b8 => [0x80489b8, 0x80489b9):
	return address in reg8
	CFA location expression: bregx(4,4)
	reg0: undefined
	reg1: undefined
	reg2: undefined
	reg3: same_value
	reg4: location expression: call_frame_cfa stack_value
	reg5: same_value
	reg6: same_value
	reg7: same_value
	reg8: location expression: call_frame_cfa plus_uconst(-4)
	reg9: undefined
EOF

testfiles testfile12-debugframe
testrun_compare ${abs_builddir}/dwarfcfi testfile12-debugframe 0x00000000000009d0 <<\EOF
0x9d0 => [0x9d0, 0x9d1):
	return address in reg16
	CFA location expression: bregx(7,8)
	reg0: same_value
	reg1: undefined
	reg2: undefined
	reg3: undefined
	reg4: undefined
	reg5: undefined
	reg6: same_value
	reg7: location expression: call_frame_cfa stack_value
	reg8: undefined
	reg9: undefined
EOF

testfiles testfileppc32-debugframe
testrun_compare ${abs_builddir}/dwarfcfi testfileppc32-debugframe 0x100004c0 <<\EOF
0x100004c0 => [0x100004c0, 0x100004d0):
	return address in reg65
	CFA location expression: bregx(1)
	reg0: undefined
	reg1: location expression: call_frame_cfa stack_value
	reg2: same_value
	reg3: undefined
	reg4: undefined
	reg5: undefined
	reg6: undefined
	reg7: undefined
	reg8: undefined
	reg9: undefined
EOF

testfiles testfileppc64-debugframe
testrun_compare ${abs_builddir}/dwarfcfi testfileppc64-debugframe 0x00000000100005b0 <<\EOF
0x100005b0 => [0x100005b0, 0x100005d0):
	return address in reg65
	CFA location expression: bregx(1)
	reg0: undefined
	reg1: location expression: call_frame_cfa stack_value
	reg2: same_value
	reg3: undefined
	reg4: undefined
	reg5: undefined
	reg6: undefined
	reg7: undefined
	reg8: undefined
	reg9: undefined
EOF

testfiles testfilearm-debugframe
testrun_compare ${abs_builddir}/dwarfcfi testfilearm-debugframe 0x00008510 <<\EOF
0x8510 => [0x8510, 0x8524):
	return address in reg14
	CFA location expression: bregx(13)
	reg0: undefined
	reg1: undefined
	reg2: undefined
	reg3: undefined
	reg4: same_value
	reg5: same_value
	reg6: same_value
	reg7: same_value
	reg8: same_value
	reg9: undefined
EOF

testfiles testfileaarch64-debugframe
testrun_compare ${abs_builddir}/dwarfcfi testfileaarch64-debugframe 0x400550 <<\EOF
0x400550 => [0x400550, 0x400568):
	return address in reg30
	CFA location expression: bregx(31)
	reg0: undefined
	reg1: undefined
	reg2: undefined
	reg3: undefined
	reg4: undefined
	reg5: undefined
	reg6: undefined
	reg7: undefined
	reg8: undefined
	reg9: undefined
EOF
