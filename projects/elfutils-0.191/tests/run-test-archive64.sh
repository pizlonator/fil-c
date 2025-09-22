#! /bin/sh
# Copyright (C) 2012, 2014 Red Hat, Inc.
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

# The test archive was produced on an s390x machine using the
# following command sequence:
#  echo 'int aaa(void){}' | gcc -x c /dev/stdin -c -o aaa.o
#  echo 'int bbb(void){} int bbb2(void){}' | gcc -x c /dev/stdin -c -o bbb.o
#  echo 'int ccc(void){} int ccc2(void){} int ccc3(void){}' \
#    | gcc -x c /dev/stdin -c -o ccc.o
#  ar cru testarchive64.a aaa.o bbb.o ccc.o
testfiles testarchive64.a

testrun_compare ${abs_top_builddir}/src/readelf -c testarchive64.a <<\EOF

Index of archive 'testarchive64.a' has 7 entries:
Archive member 'aaa.o' contains:
	aaa
Archive member 'bbb.o' contains:
	bbb
	bbb2
Archive member 'ccc.o' contains:
	ccc
	ccc2
	ccc3
EOF

testrun_compare ${abs_top_builddir}/src/nm -P -g testarchive64.a <<\EOF
testarchive64.a[aaa.o]:
aaa T 0000000000000000 0000000000000016
testarchive64.a[bbb.o]:
bbb T 0000000000000000 0000000000000016
bbb2 T 0000000000000018 0000000000000016
testarchive64.a[ccc.o]:
ccc T 0000000000000000 0000000000000016
ccc2 T 0000000000000018 0000000000000016
ccc3 T 0000000000000030 0000000000000016
EOF

exit 0
