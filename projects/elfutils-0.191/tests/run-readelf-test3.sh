#! /bin/sh
# Copyright (C) 2007 Red Hat, Inc.
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

testfiles testfile40.debug

testrun_compare ${abs_top_builddir}/src/readelf -n testfile40.debug <<\EOF

Note section [ 6] '.note' of 60 bytes at offset 0x120:
  Owner          Data size  Type
  GNU                   20  GNU_BUILD_ID
    Build ID: 34072edcd87ef6728f4b4a7956167b2fcfc3f1d3
  Linux                  4  <unknown>: 0
EOF

exit 0
