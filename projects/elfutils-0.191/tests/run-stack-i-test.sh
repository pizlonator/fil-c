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

# See run-stack-d-test.sh for dwarfinlines.cpp source.
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

# Compare with run-stack-d-test.sh to see the output without --inlines.
# Only two call frames are visible (there is a jump from main to fu or
# fubar). Explicitly use --raw so demangler support being configured in
# doesn't change the results.

# With --inlines we get all inlined calls. Note they share the same
# address.
testrun_compare ${abs_top_builddir}/src/stack -r -n 6 -i -e testfiledwarfinlines --core testfiledwarfinlines.core<<EOF
PID 13654 - core
TID 13654:
#0  0x00000000004006c8 fubar
#1  0x00000000004006c8 foobar
#2  0x00000000004006c8 bar
#3  0x00000000004006c8 foo
#4  0x00000000004006c8 _Z2fui
#5  0x00000000004004c5 main
$STACKCMD: tid 13654: shown max number of frames (6, use -n 0 for unlimited)
EOF

# With --source we can also see where in the source the inlined frames
# where originally called from.
testrun_compare ${abs_top_builddir}/src/stack -r -n 6 -s -i -e testfiledwarfinlines --core testfiledwarfinlines.core<<EOF
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
#4  0x00000000004006c8 _Z2fui
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
