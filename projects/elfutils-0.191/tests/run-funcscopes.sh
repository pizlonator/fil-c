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

testfiles testfile25

testrun_compare ${abs_builddir}/funcscopes -e testfile25 incr <<\EOF
testfile25: 0x8048000 .. 0x8049528
    inline-test.c (0x11): 0x8048348 (/home/roland/build/stock-elfutils/inline-test.c:7) .. 0x804834e (/home/roland/build/stock-elfutils/inline-test.c:9)
        incr (0x2e): 0x8048348 (/home/roland/build/stock-elfutils/inline-test.c:7) .. 0x804834e (/home/roland/build/stock-elfutils/inline-test.c:9)
            x                             [    66]
EOF

exit 0
