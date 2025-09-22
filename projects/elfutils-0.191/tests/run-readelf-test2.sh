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

testfiles testfile28

testrun_compare ${abs_top_builddir}/src/readelf -x .strtab testfile28 <<\EOF

Hex dump of section [6] '.strtab', 1 bytes at offset 0x290:
  0x00000000 00                                  .
EOF

testrun_compare ${abs_top_builddir}/src/readelf -x 6 testfile28 <<\EOF

Hex dump of section [6] '.strtab', 1 bytes at offset 0x290:
  0x00000000 00                                  .
EOF


exit 0
