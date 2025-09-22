#! /bin/sh
# Copyright (C) 2007-2009 Red Hat, Inc.
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

testfiles testfile43 testfile50

testrun_compare ${abs_builddir}/dwfl-addr-sect -e testfile43 0x64 0x8 0x98 <<\EOF
address 0x64 => module "" section 4 + 0
address 0x8 => module "" section 1 + 0x8
address 0x98 => module "" section 7 + 0
EOF

testrun_compare ${abs_builddir}/dwfl-addr-sect -e testfile50 0x1 <<\EOF
address 0x1 => module "" section 1 + 0x1
EOF

exit 0
