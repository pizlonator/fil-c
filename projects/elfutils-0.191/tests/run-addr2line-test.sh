#! /bin/sh
# Copyright (C) 2013 Red Hat, Inc.
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

testfiles testfile
tempfiles good.out stdin.nl stdin.nl.out stdin.nonl stdin.nonl.out foo.out
tempfiles addr2line.out

cat > good.out <<\EOF
foo
/home/drepper/gnu/new-bu/build/ttt/f.c:3
bar
/home/drepper/gnu/new-bu/build/ttt/b.c:4
foo
/home/drepper/gnu/new-bu/build/ttt/f.c:3
bar
/home/drepper/gnu/new-bu/build/ttt/b.c:4
foo
/home/drepper/gnu/new-bu/build/ttt/f.c:3
bar
/home/drepper/gnu/new-bu/build/ttt/b.c:4
foo
/home/drepper/gnu/new-bu/build/ttt/f.c:3
bar
/home/drepper/gnu/new-bu/build/ttt/b.c:4
EOF

echo "# Everything on the command line"
cat good.out | testrun_compare ${abs_top_builddir}/src/addr2line -f -e testfile 0x08048468 0x0804845c foo bar foo+0x0 bar+0x0 foo-0x0 bar-0x0

cat > stdin.nl <<\EOF
0x08048468
0x0804845c
foo
bar
foo+0x0
bar+0x0
foo-0x0
bar-0x0
EOF

echo "# Everything from stdin (with newlines)."
cat stdin.nl | testrun ${abs_top_builddir}/src/addr2line -f -e testfile > stdin.nl.out || exit 1
cmp good.out stdin.nl.out || exit 1

cat > foo.out <<\EOF
foo
/home/drepper/gnu/new-bu/build/ttt/f.c:3
EOF

echo "# stdin without newline address, just EOF."
echo -n "0x08048468" | testrun ${abs_top_builddir}/src/addr2line -f -e testfile > stdin.nonl.out || exit 1
cmp foo.out stdin.nonl.out || exit 1

echo "# stdin without newline symbol, just EOF."
echo -n "foo" | testrun ${abs_top_builddir}/src/addr2line -f -e testfile > stdin.nl.out || exit 1
cmp foo.out stdin.nonl.out || exit 1

tempfiles good.addr.out

cat > good.addr.out <<\EOF
0x08048468
foo
/home/drepper/gnu/new-bu/build/ttt/f.c:3
0x0804845c
bar
/home/drepper/gnu/new-bu/build/ttt/b.c:4
0x08048468
foo
/home/drepper/gnu/new-bu/build/ttt/f.c:3
0x0804845c
bar
/home/drepper/gnu/new-bu/build/ttt/b.c:4
0x08048468
foo
/home/drepper/gnu/new-bu/build/ttt/f.c:3
0x0804845c
bar
/home/drepper/gnu/new-bu/build/ttt/b.c:4
0x08048468
foo
/home/drepper/gnu/new-bu/build/ttt/f.c:3
0x0804845c
bar
/home/drepper/gnu/new-bu/build/ttt/b.c:4
EOF

echo "# Everything on the command line with addresses"
cat good.addr.out | testrun_compare ${abs_top_builddir}/src/addr2line -a -f -e testfile 0x08048468 0x0804845c foo bar foo+0x0 bar+0x0 foo-0x0 bar-0x0

echo "# Everything from stdin (with newlines) with addresses."
cat stdin.nl | testrun ${abs_top_builddir}/src/addr2line -a -f -e testfile > stdin.nl.out || exit 1
cmp good.addr.out stdin.nl.out || exit 1

echo "# Pretty with functions and addresses."
testrun_compare ${abs_top_builddir}/src/addr2line --pretty -a -f -e testfile 0x08048468 0x0804845c << EOF
0x08048468: foo at /home/drepper/gnu/new-bu/build/ttt/f.c:3
0x0804845c: bar at /home/drepper/gnu/new-bu/build/ttt/b.c:4
EOF

exit 0
