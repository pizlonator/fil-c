#! /usr/bin/env bash
# Copyright (C) 2022 Mark J. Wielaard <mark@klomp.org>
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

tempfiles testfile test.ar

echo create test.ar with 3 testfile
echo 1 > testfile
testrun ${abs_top_builddir}/src/ar -vr test.ar testfile
echo 2 > testfile
testrun ${abs_top_builddir}/src/ar -vq test.ar testfile
testrun ${abs_top_builddir}/src/ar -t test.ar
echo 3 > testfile
testrun ${abs_top_builddir}/src/ar -vq test.ar testfile
testrun_compare ${abs_top_builddir}/src/ar -t test.ar << EOF
testfile
testfile
testfile
EOF

echo list content of testfile 1 2 3
testrun ${abs_top_builddir}/src/ar -vx -N 1 test.ar testfile
diff -u testfile - << EOF
1
EOF
testrun ${abs_top_builddir}/src/ar -vx -N 2 test.ar testfile
diff -u testfile - << EOF
2
EOF
testrun ${abs_top_builddir}/src/ar -vx -N 3 test.ar testfile
diff -u testfile - << EOF
3
EOF

echo delete testfile 2
testrun ${abs_top_builddir}/src/ar -vd -N 2 test.ar testfile
testrun_compare ${abs_top_builddir}/src/ar -t test.ar << EOF
testfile
testfile
EOF
testrun ${abs_top_builddir}/src/ar -vx -N 1 test.ar testfile
diff -u testfile - << EOF
1
EOF
testrun ${abs_top_builddir}/src/ar -vx -N 2 test.ar testfile
diff -u testfile - << EOF
3
EOF

exit 0
