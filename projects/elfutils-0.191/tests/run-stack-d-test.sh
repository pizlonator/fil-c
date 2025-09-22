#! /bin/sh
# Copyright (C) 2014, 2015 Red Hat, Inc.
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

# // g++ dwarfinlines.cpp -g -o testfiledwarfinlines -O2
# int
# fubar (int x)
# {
#   __asm__ ( "nop" ::: );
#   return 42 / x - 2;
# }
#
# void foobar (int z1, int z2)
# {
#   __asm__ ( "nop" ::: );
#   int x = z1 + z2;
#   while (z1 + x + 1 != 42)
#     x = fubar (z1 + z2 + x);
# }
#
# void bar (int z)
# {
#   int a, b;
#   a = b = z / 2;
#   foobar(a, b);
# }
#
# void foo (int x)
# {
#   if (x > 0)
#     bar(x - 2);
# }
#
# void fu (int y)
# {
#   __asm__ ( "nop" ::: );
#   foo (y + 1);
# }
#
# int
# main (int argc, char **argv)
# {
#   fu (argc);
# }
testfiles testfiledwarfinlines testfiledwarfinlines.core

# Depending on whether we are running make check or make installcheck
# the actual binary name under test might be different. It is used in
# the error message, which we also try to match.
if test "$elfutils_testrun" = "installed"; then
STACKCMD=${bindir}/`program_transform stack`
else
STACKCMD=${abs_top_builddir}/src/stack
fi

# Disable valgrind while dumping because of a bug unmapping libc.so.
# https://bugs.kde.org/show_bug.cgi?id=327427
SAVED_VALGRIND_CMD="$VALGRIND_CMD"
unset VALGRIND_CMD

# Without -d the top function comes out as fu. Use --raw to not demangle.
testrun_compare ${abs_top_builddir}/src/stack -r -n 2 -e testfiledwarfinlines --core testfiledwarfinlines.core<<EOF
PID 13654 - core
TID 13654:
#0  0x00000000004006c8 _Z2fui
#1  0x00000000004004c5 main
$STACKCMD: tid 13654: shown max number of frames (2, use -n 0 for unlimited)
EOF

# But when asking for source we see it is actually on line 6.
# (Which is in function fubar, not fu). Use --raw to not demangle.
testrun_compare ${abs_top_builddir}/src/stack -r -n 2 -s -e testfiledwarfinlines --core testfiledwarfinlines.core<<EOF
PID 13654 - core
TID 13654:
#0  0x00000000004006c8 _Z2fui
    /home/mark/src/tests/dwarfinlines.cpp:6
#1  0x00000000004004c5 main
    /home/mark/src/tests/dwarfinlines.cpp:39
$STACKCMD: tid 13654: shown max number of frames (2, use -n 0 for unlimited)
EOF

# So with --debugname we get the function correct as fubar.
testrun_compare ${abs_top_builddir}/src/stack -n 2 -d -e testfiledwarfinlines --core testfiledwarfinlines.core<<EOF
PID 13654 - core
TID 13654:
#0  0x00000000004006c8 fubar
#1  0x00000000004004c5 main
$STACKCMD: tid 13654: shown max number of frames (2, use -n 0 for unlimited)
EOF

# Which now matches the source line (again 6 of course).
testrun_compare ${abs_top_builddir}/src/stack -n 2 -s -d -e testfiledwarfinlines --core testfiledwarfinlines.core<<EOF
PID 13654 - core
TID 13654:
#0  0x00000000004006c8 fubar
    /home/mark/src/tests/dwarfinlines.cpp:6
#1  0x00000000004004c5 main
    /home/mark/src/tests/dwarfinlines.cpp:39
$STACKCMD: tid 13654: shown max number of frames (2, use -n 0 for unlimited)
EOF

if [ "x$SAVED_VALGRIND_CMD" != "x" ]; then
  VALGRIND_CMD="$SAVED_VALGRIND_CMD"
  export VALGRIND_CMD
fi

exit 0
