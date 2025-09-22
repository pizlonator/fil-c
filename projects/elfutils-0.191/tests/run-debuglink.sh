#! /bin/sh
# Copyright (C) 2014 Red Hat, Inc.
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

# Just some random testfiles, two with, one without .gnu_debuglink
testfiles testfile36 testfile52-32.so testfile42

testrun_compare  ${abs_builddir}/debuglink testfile36 testfile52-32.so testfile42 <<\EOF
testfile36: testfile36.debug, crc: 8c5c20a3
testfile52-32.so: testfile52-32.so.debug, crc: b835a71d
testfile42: <no gnu_debuglink file>
EOF

exit 0
