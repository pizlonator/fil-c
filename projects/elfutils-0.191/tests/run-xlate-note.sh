# Copyright (C) 2019 Red Hat, Inc.
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

testfiles testfileppc32
testrun_compare ${abs_top_builddir}/tests/xlate_notes testfileppc32 << EOF
Notes in section 2:
type: 1,1, namesz: 4,4, descsz: 16,16
Notes in section 3:
type: 3,3, namesz: 4,4, descsz: 20,20
EOF

testfiles testfileppc64
testrun_compare ${abs_top_builddir}/tests/xlate_notes testfileppc64 << EOF
Notes in section 2:
type: 1,1, namesz: 4,4, descsz: 16,16
Notes in section 3:
type: 3,3, namesz: 4,4, descsz: 20,20
EOF

testfiles testfiles390
testrun_compare ${abs_top_builddir}/tests/xlate_notes testfiles390 << EOF
Notes in section 2:
type: 1,1, namesz: 4,4, descsz: 16,16
Notes in section 3:
type: 3,3, namesz: 4,4, descsz: 20,20
EOF

testfiles testfiles390x
testrun_compare ${abs_top_builddir}/tests/xlate_notes testfiles390x << EOF
Notes in section 2:
type: 1,1, namesz: 4,4, descsz: 16,16
Notes in section 3:
type: 3,3, namesz: 4,4, descsz: 20,20
EOF

testfiles testfileaarch64
testrun_compare ${abs_top_builddir}/tests/xlate_notes testfileaarch64 << EOF
Notes in section 2:
type: 1,1, namesz: 4,4, descsz: 16,16
Notes in section 3:
type: 3,3, namesz: 4,4, descsz: 20,20
EOF

testfiles testfilearm
testrun_compare ${abs_top_builddir}/tests/xlate_notes testfilearm << EOF
Notes in section 2:
type: 1,1, namesz: 4,4, descsz: 16,16
Notes in section 3:
type: 3,3, namesz: 4,4, descsz: 20,20
EOF

testfiles testfile_gnu_props.32be.o
testrun_compare ${abs_top_builddir}/tests/xlate_notes testfile_gnu_props.32be.o << EOF
Notes in section 4:
type: 5,5, namesz: 4,4, descsz: 12,12
type: 5,5, namesz: 4,4, descsz: 8,8
EOF

testfiles testfile_gnu_props.32le.o
testrun_compare ${abs_top_builddir}/tests/xlate_notes testfile_gnu_props.32le.o << EOF
Notes in section 4:
type: 5,5, namesz: 4,4, descsz: 12,12
type: 5,5, namesz: 4,4, descsz: 8,8
EOF

testfiles testfile_gnu_props.64be.o
testrun_compare ${abs_top_builddir}/tests/xlate_notes testfile_gnu_props.64be.o << EOF
Notes in section 4:
type: 5,5, namesz: 4,4, descsz: 16,16
type: 5,5, namesz: 4,4, descsz: 8,8
EOF

testfiles testfile_gnu_props.64le.o
testrun_compare ${abs_top_builddir}/tests/xlate_notes testfile_gnu_props.64le.o << EOF
Notes in section 4:
type: 5,5, namesz: 4,4, descsz: 16,16
type: 5,5, namesz: 4,4, descsz: 8,8
EOF
