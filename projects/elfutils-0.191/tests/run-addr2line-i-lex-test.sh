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

# // g++ x.cpp -g -fPIC -olibx.so -shared -O3 -fvisibility=hidden
#
# void foobar()
# {
#   __asm__ ( "nop" ::: );
# }
#
# void foo()
# {
#   {
#     void (*bar) () = foobar;
#     bar();
#   }
# }

testfiles testfile-lex-inlines

testrun_compare ${abs_top_builddir}/src/addr2line -i -e testfile-lex-inlines 0x0000000000000680 <<\EOF
/tmp/x.cpp:5
EOF

testrun_compare ${abs_top_builddir}/src/addr2line -i -e testfile-lex-inlines 0x0000000000000681 <<\EOF
/tmp/x.cpp:5
EOF

testrun_compare ${abs_top_builddir}/src/addr2line -i -e testfile-lex-inlines 0x0000000000000690 <<\EOF
/tmp/x.cpp:5
/tmp/x.cpp:12
EOF

testrun_compare ${abs_top_builddir}/src/addr2line -i -e testfile-lex-inlines 0x0000000000000691 <<\EOF
/tmp/x.cpp:5
/tmp/x.cpp:12
EOF

# All together now (plus function names).
testrun_compare ${abs_top_builddir}/src/addr2line -f -i -e testfile-lex-inlines 0x0000000000000680 0x0000000000000681 0x0000000000000690 0x0000000000000691 <<\EOF
_Z6foobarv
/tmp/x.cpp:5
_Z6foobarv
/tmp/x.cpp:5
foobar inlined at /tmp/x.cpp:12 in _Z3foov
/tmp/x.cpp:5
_Z3foov
/tmp/x.cpp:12
foobar inlined at /tmp/x.cpp:12 in _Z3foov
/tmp/x.cpp:5
_Z3foov
/tmp/x.cpp:12
EOF

exit 0
