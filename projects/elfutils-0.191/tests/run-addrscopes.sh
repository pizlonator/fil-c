#! /bin/sh
# Copyright (C) 2005, 2014 Red Hat, Inc.
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

testfiles testfile22

testrun_compare ${abs_builddir}/addrscopes -e testfile22 0x8048353 <<\EOF
0x8048353:
    tests/foo.c (0x11): 0x8048348 (tests/foo.c:5) .. 0x804837c (tests/foo.c:16)
        global                        [    be]
        function (0x2e): 0x8048348 (tests/foo.c:5) .. 0x8048359 (tests/foo.c:11)
            local                         [    8f]
EOF

test_cleanup

testfiles testfile24
testrun_compare ${abs_builddir}/addrscopes -e testfile24 0x804834e <<\EOF
0x804834e:
    inline-test.c (0x11): 0x8048348 (/home/roland/build/stock-elfutils/inline-test.c:7) .. 0x8048360 (/home/roland/build/stock-elfutils/inline-test.c:16)
        add (0x1d): 0x804834e (/home/roland/build/stock-elfutils/inline-test.c:3) .. 0x804834f
            y                             [    9d]
            x                             [    a2]
            x (abstract)
            y (abstract)
EOF

exit 0
