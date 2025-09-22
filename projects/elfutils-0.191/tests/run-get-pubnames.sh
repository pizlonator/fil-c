#! /bin/sh
# Copyright (C) 1999, 2000, 2002, 2003, 2005 Red Hat, Inc.
# This file is part of elfutils.
# Written by Ulrich Drepper <drepper@redhat.com>, 1999.
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

testfiles testfile testfile2

testrun_compare ${abs_builddir}/get-pubnames testfile testfile2 <<\EOF
 [ 0] "main", die: 104, cu: 11
CU name: "m.c"
object name: "main"
 [ 1] "a", die: 174, cu: 11
CU name: "m.c"
object name: "a"
 [ 2] "bar", die: 295, cu: 202
CU name: "b.c"
object name: "bar"
 [ 3] "foo", die: 5721, cu: 5628
CU name: "f.c"
object name: "foo"
 [ 0] "bar", die: 72, cu: 11
CU name: "b.c"
object name: "bar"
 [ 1] "foo", die: 2490, cu: 2429
CU name: "f.c"
object name: "foo"
 [ 2] "main", die: 2593, cu: 2532
CU name: "m.c"
object name: "main"
 [ 3] "a", die: 2663, cu: 2532
CU name: "m.c"
object name: "a"
EOF

exit 0
