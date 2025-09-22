#! /bin/sh
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


# Like run-typeiter.sh but we first add many sections to make sure
# dwarf_begin actually recognizes the debug section names.
testfiles testfile-debug-types

testrun ${abs_builddir}/addsections 65535 testfile-debug-types
testrun_compare ${abs_builddir}/typeiter2 testfile-debug-types <<\EOF
ok A [68]
ok B [38]
EOF

exit 0
