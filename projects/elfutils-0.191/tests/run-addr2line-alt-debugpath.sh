#! /bin/sh
# Copyright (C) 2015 Red Hat, Inc.
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

# See run-addr2line-i-test.sh
testfiles testfile-inlines

# Split off the debuginfo and put it under a separate subdir from the
# original binary.  Use --debuginfo-path to connect the dots again.
# Note that we use separate subdirs/roots for the binaries and debug files.
abs_test_bindir=$(pwd)/bindir
abs_test_debugdir=$(pwd)/debugdir

mkdir ${abs_test_bindir}
mkdir ${abs_test_bindir}/bin
mkdir ${abs_test_debugdir}
mkdir ${abs_test_debugdir}/bin

testrun ${abs_top_builddir}/src/strip -f ${abs_test_debugdir}/bin/testfile-inlines.debug -o ${abs_test_bindir}/bin/testfile-inlines testfile-inlines

# Can we find the separate debuginfo file now?
testrun_compare ${abs_top_builddir}/src/addr2line --pretty-print -a -f -i -e ${abs_test_bindir}/bin/testfile-inlines --debuginfo-path=${abs_test_debugdir} 0x00000000000005a0 0x00000000000005a1 0x00000000000005b0 0x00000000000005b1 0x00000000000005c0 0x00000000000005d0 0x00000000000005e0 0x00000000000005e1 0x00000000000005f0 0x00000000000005f1 0x00000000000005f2 <<\EOF
0x00000000000005a0: foobar at /tmp/x.cpp:5
0x00000000000005a1: foobar at /tmp/x.cpp:6
0x00000000000005b0: fubar at /tmp/x.cpp:10
0x00000000000005b1: fubar at /tmp/x.cpp:11
0x00000000000005c0: foobar at /tmp/x.cpp:5
 (inlined by) bar at /tmp/x.cpp:15
0x00000000000005d0: fubar at /tmp/x.cpp:10
 (inlined by) baz at /tmp/x.cpp:20
0x00000000000005e0: foobar at /tmp/x.cpp:5
 (inlined by) bar at /tmp/x.cpp:15
 (inlined by) _Z3foov at /tmp/x.cpp:25
0x00000000000005e1: fubar at /tmp/x.cpp:10
 (inlined by) baz at /tmp/x.cpp:20
 (inlined by) _Z3foov at /tmp/x.cpp:26
0x00000000000005f0: _Z2fuv at /tmp/x.cpp:31
0x00000000000005f1: fubar at /tmp/x.cpp:10
 (inlined by) _Z2fuv at /tmp/x.cpp:32
0x00000000000005f2: foobar at /tmp/x.cpp:5
 (inlined by) _Z2fuv at /tmp/x.cpp:33
EOF

# Cleanup
rm ${abs_test_bindir}/bin/testfile-inlines
rm ${abs_test_debugdir}/bin/testfile-inlines.debug
rmdir ${abs_test_bindir}/bin
rmdir ${abs_test_bindir}
rmdir ${abs_test_debugdir}/bin
rmdir ${abs_test_debugdir}

exit 0
