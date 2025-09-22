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

testfiles testfile36 testfile36.debug

testrun_compare ${abs_builddir}/dwflmodtest -e testfile36 <<\EOF
module:                                00000000..00002308 testfile36 (null)
module:                                00000000 DWARF 0 (no error)
module:                                00000000..00002308 testfile36 testfile36.debug
EOF

exit 0
