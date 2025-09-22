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

# See run-readelf-dwz-multi.sh
testfiles libtestfile_multi_shared.so testfile_multi_main testfile_multi.dwz
testfiles testfile-dwzstr testfile-dwzstr.multi

testrun_compare ${abs_builddir}/allfcts testfile_multi_main libtestfile_multi_shared.so testfile-dwzstr <<\EOF
/home/mark/src/tests/dwz/main.c:3:main
/home/mark/src/tests/dwz/shared.c:3:call_foo
/home/mark/src/tests/main.c:8:main
EOF

# - test-offset-loop.c
#
# #include <stdbool.h>
# #include <string.h>
# #include <errno.h>
# void padding (int x, int y, int z) { }
# static inline bool is_error (int err) { return err != 0; }
# static inline int get_errno (void) { return errno; }
# int main () { return is_error (get_errno ()); }
#
# gcc -g -O2 test-offset-loop.c -o test-offset-loop
# cp test-offset-loop test-offset-loop2
# dwz test-offset-loop test-offset-loop2 -m test-offset-loop.alt

testfiles test-offset-loop test-offset-loop.alt
tempfiles allfcts.out

# Use head to capture output because the output could be infinite...
testrun ${abs_builddir}/allfcts test-offset-loop | head -n 20 > allfcts.out
testrun_compare cat allfcts.out <<\EOF
/tmp/test-offset-loop.c:6:get_errno
/tmp/test-offset-loop.c:5:is_error
/tmp/test-offset-loop.c:4:padding
/tmp/test-offset-loop.c:7:main
EOF

# allfcts has a too simple mechanism for setting the alt file.
# check that if we don't set it, things still work (because libdw will
# find the alt file for us).
mkdir subdir
mv test-offset-loop test-offset-loop.alt subdir/
testrun ${abs_builddir}/allfcts subdir/test-offset-loop > allfcts.out
testrun_compare cat allfcts.out <<\EOF
Warning: no alt file found.
/tmp/test-offset-loop.c:6:get_errno
/tmp/test-offset-loop.c:5:is_error
/tmp/test-offset-loop.c:4:padding
/tmp/test-offset-loop.c:7:main
EOF

rm -rf subdir

exit 0
