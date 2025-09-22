#! /bin/sh
# Test for dwarf_next_cfi.
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

# Test files come from run-addrcfi
testfiles testfile11 testfile12
testfiles testfilearm testfileaarch64
testfiles testfileppc32 testfileppc64

testrun_compare ${abs_builddir}/next_cfi testfile11 <<\EOF
testfile11
.eh_frame
[0] CIE augmentation="zPL"
[28] FDE cie=[0]
[64] FDE cie=[0]
[96] FDE cie=[0]
[128] FDE cie=[0]
.debug_frame
[0] CIE augmentation=""
[20] FDE cie=[0]
[52] FDE cie=[0]
[76] FDE cie=[0]
[100] FDE cie=[0]
[124] FDE cie=[0]
[148] FDE cie=[0]
[172] FDE cie=[0]
[196] FDE cie=[0]
EOF

testrun_compare ${abs_builddir}/next_cfi testfile12 <<\EOF
testfile12
.eh_frame
[0] CIE augmentation=""
[16] CIE augmentation="zR"
[40] FDE cie=[16]
.debug_frame
[0] CIE augmentation=""
[24] FDE cie=[0]
EOF

testrun_compare ${abs_builddir}/next_cfi testfilearm <<\EOF
testfilearm
.eh_frame
.debug_frame
[0] CIE augmentation=""
[16] FDE cie=[0]
[32] CIE augmentation=""
[48] FDE cie=[32]
EOF

testrun_compare ${abs_builddir}/next_cfi testfileaarch64 <<\EOF
testfileaarch64
.eh_frame
[0] CIE augmentation="zR"
[24] FDE cie=[0]
[80] FDE cie=[0]
.debug_frame
[0] CIE augmentation=""
[16] FDE cie=[0]
[40] CIE augmentation=""
[56] FDE cie=[40]
EOF

testrun_compare ${abs_builddir}/next_cfi testfileppc32 <<\EOF
testfileppc32
.eh_frame
[0] CIE augmentation="zR"
[20] FDE cie=[0]
[40] FDE cie=[0]
[96] FDE cie=[0]
.debug_frame
[0] CIE augmentation=""
[16] FDE cie=[0]
[32] CIE augmentation=""
[48] FDE cie=[32]
EOF

testrun_compare ${abs_builddir}/next_cfi testfileppc64 <<\EOF
testfileppc64
.eh_frame
[0] CIE augmentation="zR"
[20] FDE cie=[0]
[40] FDE cie=[0]
[64] CIE augmentation="zR"
[88] FDE cie=[64]
[144] FDE cie=[64]
.debug_frame
[0] CIE augmentation=""
[16] FDE cie=[0]
[56] CIE augmentation=""
[72] FDE cie=[56]
EOF
