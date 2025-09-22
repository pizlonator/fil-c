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

if test -n "$ELFUTILS_DISABLE_DEMANGLE"; then
  echo "demangler unsupported"
  exit 77
fi

. $srcdir/test-subr.sh

# See run-stack-d-test.sh and run-stack-i-test.sh
# Same tests, now with demangler support, no -r, and without -d.
# Only change in output is an explicit fu(int) instead of _Z2fui.

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

# Without -d the top function comes out as fu.
testrun_compare ${abs_top_builddir}/src/stack -n 2 -e testfiledwarfinlines --core testfiledwarfinlines.core<<EOF
PID 13654 - core
TID 13654:
#0  0x00000000004006c8 fu(int)
#1  0x00000000004004c5 main
$STACKCMD: tid 13654: shown max number of frames (2, use -n 0 for unlimited)
EOF

# But when asking for source we see it is actually on line 6.
# (Which is in function fubar, not fu).
testrun_compare ${abs_top_builddir}/src/stack -n 2 -s -e testfiledwarfinlines --core testfiledwarfinlines.core<<EOF
PID 13654 - core
TID 13654:
#0  0x00000000004006c8 fu(int)
    /home/mark/src/tests/dwarfinlines.cpp:6
#1  0x00000000004004c5 main
    /home/mark/src/tests/dwarfinlines.cpp:39
$STACKCMD: tid 13654: shown max number of frames (2, use -n 0 for unlimited)
EOF

# With --inlines we get all inlined calls. Note they share the same
# address.
testrun_compare ${abs_top_builddir}/src/stack -n 6 -i -e testfiledwarfinlines --core testfiledwarfinlines.core<<EOF
PID 13654 - core
TID 13654:
#0  0x00000000004006c8 fubar
#1  0x00000000004006c8 foobar
#2  0x00000000004006c8 bar
#3  0x00000000004006c8 foo
#4  0x00000000004006c8 fu(int)
#5  0x00000000004004c5 main
$STACKCMD: tid 13654: shown max number of frames (6, use -n 0 for unlimited)
EOF

# With --source we can also see where in the source the inlined frames
# where originally called from.
testrun_compare ${abs_top_builddir}/src/stack -n 6 -s -i -e testfiledwarfinlines --core testfiledwarfinlines.core<<EOF
PID 13654 - core
TID 13654:
#0  0x00000000004006c8 fubar
    /home/mark/src/tests/dwarfinlines.cpp:6
#1  0x00000000004006c8 foobar
    /home/mark/src/tests/dwarfinlines.cpp:14
#2  0x00000000004006c8 bar
    /home/mark/src/tests/dwarfinlines.cpp:21
#3  0x00000000004006c8 foo
    /home/mark/src/tests/dwarfinlines.cpp:27
#4  0x00000000004006c8 fu(int)
    /home/mark/src/tests/dwarfinlines.cpp:33
#5  0x00000000004004c5 main
    /home/mark/src/tests/dwarfinlines.cpp:39
$STACKCMD: tid 13654: shown max number of frames (6, use -n 0 for unlimited)
EOF

if [ "x$SAVED_VALGRIND_CMD" != "x" ]; then
  VALGRIND_CMD="$SAVED_VALGRIND_CMD"
  export VALGRIND_CMD
fi

exit 0
