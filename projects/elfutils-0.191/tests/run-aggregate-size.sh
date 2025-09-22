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

# char c;
# int i;
# long l;
#
# void *v;
#
# struct s
# {
#   char *a;
#   int i;
# } s;
#
# char ca[16];
# int ia[32];
# void *va[64];
# struct s sa[8];

# On x86_64 (LP64):
# gcc -g -c -o testfile-sizes1.o sizes.c
# clang -g -c -o testfile-sizes2.o sizes.c

# const char c;
# volatile int i;
# const volatile long l;
#
# void * restrict v;
#
# struct s
# {
#   const char *a;
#   volatile int i;
# } s;
#
# const char ca[16];
# volatile int ia[32];
# const volatile void * const volatile restrict va[64];
# struct s sa[8];
# double d3d[3][4][5];
#
# typedef const int foo;
# typedef volatile foo bar;
# foo f;
# bar b;
#
# gcc -std=c99 -g -c -o testfile-sizes3.o sizes.c

# The file testfile-size4.o is hand-crafted.

testfiles testfile-sizes1.o testfile-sizes2.o testfile-sizes3.o testfile-sizes4.o

testrun_compare ${abs_builddir}/aggregate_size -e testfile-sizes1.o <<\EOF
c size 1
i size 4
l size 8
v size 8
s size 16
ca size 16
ia size 128
va size 512
sa size 128
EOF

testrun_compare ${abs_builddir}/aggregate_size -e testfile-sizes2.o <<\EOF
c size 1
i size 4
l size 8
v size 8
s size 16
ca size 16
ia size 128
va size 512
sa size 128
EOF

testrun_compare ${abs_builddir}/aggregate_size -e testfile-sizes3.o <<\EOF
c size 1
i size 4
l size 8
v size 8
s size 16
ca size 16
ia size 128
va size 512
sa size 128
d3d size 480
f size 4
b size 4
EOF

testrun_compare ${abs_builddir}/aggregate_size -e testfile-sizes4.o <<\EOF
v size 257
EOF

exit 0
