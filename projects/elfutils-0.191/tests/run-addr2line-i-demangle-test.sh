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

if test -n "$ELFUTILS_DISABLE_DEMANGLE"; then
  echo "demangler unsupported"
  exit 77
fi

. $srcdir/test-subr.sh

# See run-addr2line-i-test.sh for how to generate test files.
testfiles testfile-inlines

# All together now plus (demangled) function names.
testrun_compare ${abs_top_builddir}/src/addr2line -C -f -i -e testfile-inlines 0x00000000000005a0 0x00000000000005a1 0x00000000000005b0 0x00000000000005b1 0x00000000000005c0 0x00000000000005d0 0x00000000000005e0 0x00000000000005e1 0x00000000000005f0 0x00000000000005f1 0x00000000000005f2 <<\EOF
foobar
/tmp/x.cpp:5
foobar
/tmp/x.cpp:6
fubar
/tmp/x.cpp:10
fubar
/tmp/x.cpp:11
foobar inlined at /tmp/x.cpp:15 in bar()
/tmp/x.cpp:5
bar
/tmp/x.cpp:15
fubar inlined at /tmp/x.cpp:20 in baz()
/tmp/x.cpp:10
baz
/tmp/x.cpp:20
foobar inlined at /tmp/x.cpp:15 in foo()
/tmp/x.cpp:5
bar
/tmp/x.cpp:15
foo()
/tmp/x.cpp:25
fubar inlined at /tmp/x.cpp:20 in foo()
/tmp/x.cpp:10
baz
/tmp/x.cpp:20
foo()
/tmp/x.cpp:26
fu()
/tmp/x.cpp:31
fubar inlined at /tmp/x.cpp:32 in fu()
/tmp/x.cpp:10
fu()
/tmp/x.cpp:32
foobar inlined at /tmp/x.cpp:33 in fu()
/tmp/x.cpp:5
fu()
/tmp/x.cpp:33
EOF

exit 0
