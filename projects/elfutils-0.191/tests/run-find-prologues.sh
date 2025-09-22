#! /bin/sh
# Copyright (C) 2005 Red Hat, Inc.
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

testfiles testfile testfile11 testfile22 testfile24 \
	  testfile25 testfile3 testfile4 testfile5 testfile6

testrun_compare ${abs_builddir}/find-prologues -e testfile <<\EOF
main             0x000000000804842c 0x0000000008048432
bar              0x000000000804845c 0x000000000804845f
foo              0x0000000008048468 0x000000000804846b
EOF

testrun_compare ${abs_builddir}/find-prologues -e testfile11 <<\EOF
main             0x00000000080489b8 0x00000000080489cd
gnu_obj_2        0x0000000008048c9e 0x0000000008048ca4
gnu_obj_3        0x0000000008048cd8 0x0000000008048cde
gnu_obj_2        0x0000000008048cf4 0x0000000008048cfa
~invalid_argument 0x0000000008048d2e 0x0000000008048d34
gnu_obj_1        0x0000000008048d62 0x0000000008048d65
gnu_obj_1        0x0000000008048d8a 0x0000000008048d8d
~invalid_argument 0x0000000008048db2 0x0000000008048db8
EOF

testrun_compare ${abs_builddir}/find-prologues -e testfile22 <<\EOF
function         0x0000000008048348 0x000000000804834e
main             0x000000000804835b 0x0000000008048377
EOF

testrun_compare ${abs_builddir}/find-prologues -e testfile24 <<\EOF
incr             0x0000000008048348 0x000000000804834e
main             0x0000000008048354 0x0000000008048360
EOF

testrun_compare ${abs_builddir}/find-prologues -e testfile25 <<\EOF
incr             0x0000000008048348 0x000000000804834c
EOF

testrun_compare ${abs_builddir}/find-prologues -e testfile3 <<\EOF
main             0x000000000804842c 0x0000000008048433
bar              0x0000000008048458 0x000000000804845b
foo              0x0000000008048464 0x0000000008048467
EOF

testrun_compare ${abs_builddir}/find-prologues -e testfile4 <<\EOF
get              0x00000000080493fc 0x0000000008049402
main             0x0000000008049498 0x000000000804949e
a                0x000000000804d85c 0x000000000804d85c
__tfPCc          0x000000000804d86c 0x000000000804d872
__tfCc           0x000000000804d8a4 0x000000000804d8a4
EOF

testrun_compare ${abs_builddir}/find-prologues -e testfile5 <<\EOF
bar              0x000000000804842c 0x000000000804842f
foo              0x0000000008048438 0x000000000804843b
main             0x0000000008048444 0x000000000804844a
EOF

testrun_compare ${abs_builddir}/find-prologues -e testfile6 <<\EOF
main             0x00000000080489b8 0x00000000080489cd
gnu_obj_2        0x0000000008048c9e 0x0000000008048ca4
gnu_obj_3        0x0000000008048cd8 0x0000000008048cde
gnu_obj_2        0x0000000008048cf4 0x0000000008048cfa
~invalid_argument 0x0000000008048d2e 0x0000000008048d34
gnu_obj_1        0x0000000008048d62 0x0000000008048d65
gnu_obj_1        0x0000000008048d8a 0x0000000008048d8d
~invalid_argument 0x0000000008048db2 0x0000000008048db8
EOF

exit 0
