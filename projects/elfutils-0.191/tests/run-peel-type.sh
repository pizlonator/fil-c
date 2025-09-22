#! /bin/sh
# Copyright (C) 2017 Red Hat, Inc.
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

# See run-aggregate-size.sh for how to generate testfiles.

testfiles testfile-sizes1.o testfile-sizes2.o testfile-sizes3.o

testrun_compare ${abs_builddir}/peel_type -e testfile-sizes1.o <<\EOF
c raw type base_type
i raw type base_type
l raw type base_type
v raw type pointer_type
s raw type structure_type
ca raw type array_type
ia raw type array_type
va raw type array_type
sa raw type array_type
EOF

testrun_compare ${abs_builddir}/peel_type -e testfile-sizes2.o <<\EOF
c raw type base_type
i raw type base_type
l raw type base_type
v raw type pointer_type
s raw type structure_type
ca raw type array_type
ia raw type array_type
va raw type array_type
sa raw type array_type
EOF

testrun_compare ${abs_builddir}/peel_type -e testfile-sizes3.o <<\EOF
c raw type base_type
i raw type base_type
l raw type base_type
v raw type pointer_type
s raw type structure_type
ca raw type array_type
ia raw type array_type
va raw type array_type
sa raw type array_type
d3d raw type array_type
f raw type base_type
b raw type base_type
EOF

exit 0
