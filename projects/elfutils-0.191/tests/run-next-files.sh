#! /bin/sh
# Variant of run-get-files that uses dwarf_next_lines.
# Copyright (C) 2018 Red Hat, Inc.
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

testfiles testfile testfile2

testrun_compare ${abs_builddir}/next-files testfile testfile2 <<\EOF
off = 0
 dirs[0] = "/home/drepper/gnu/new-bu/build/ttt"
 file[0] = "???"
 file[1] = "/home/drepper/gnu/new-bu/build/ttt/m.c"
off = 75
 dirs[0] = "/home/drepper/gnu/new-bu/build/ttt"
 file[0] = "???"
 file[1] = "/home/drepper/gnu/new-bu/build/ttt/b.c"
 file[2] = "/usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h"
 file[3] = "/usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h"
 file[4] = "/usr/include/bits/types.h"
 file[5] = "/usr/include/bits/sched.h"
 file[6] = "/usr/include/bits/pthreadtypes.h"
 file[7] = "/usr/include/stdio.h"
 file[8] = "/usr/include/libio.h"
 file[9] = "/usr/include/wchar.h"
 file[10] = "/usr/include/_G_config.h"
 file[11] = "/usr/include/gconv.h"
off = 480
 dirs[0] = "/home/drepper/gnu/new-bu/build/ttt"
 file[0] = "???"
 file[1] = "/home/drepper/gnu/new-bu/build/ttt/f.c"
off = 0
 dirs[0] = "/shoggoth/drepper"
 file[0] = "???"
 file[1] = "/shoggoth/drepper/b.c"
 file[2] = "/home/geoffk/objs/laurel-000912-branch/lib/gcc-lib/powerpc-unknown-linux-gnu/2.96-laurel-000912/include/stddef.h"
 file[3] = "/home/geoffk/objs/laurel-000912-branch/lib/gcc-lib/powerpc-unknown-linux-gnu/2.96-laurel-000912/include/stdarg.h"
 file[4] = "/shoggoth/drepper/<built-in>"
 file[5] = "/usr/include/bits/types.h"
 file[6] = "/usr/include/stdio.h"
 file[7] = "/usr/include/libio.h"
 file[8] = "/usr/include/_G_config.h"
off = 418
 dirs[0] = "/shoggoth/drepper"
 file[0] = "???"
 file[1] = "/shoggoth/drepper/f.c"
off = 485
 dirs[0] = "/shoggoth/drepper"
 file[0] = "???"
 file[1] = "/shoggoth/drepper/m.c"
EOF

# see tests/testfile-dwarf-45.source
testfiles testfile-splitdwarf-4 testfile-hello4.dwo testfile-world4.dwo
testfiles testfile-splitdwarf-5 testfile-hello5.dwo testfile-world5.dwo

testrun_compare ${abs_builddir}/next-files testfile-splitdwarf-4 testfile-hello4.dwo testfile-world4.dwo <<\EOF
off = 0
 dirs[0] = "/home/mark/src/elfutils/tests"
 dirs[1] = "/opt/local/install/gcc/lib/gcc/x86_64-pc-linux-gnu/9.0.0/include"
 file[0] = "???"
 file[1] = "/home/mark/src/elfutils/tests/hello.c"
 file[2] = "/home/mark/src/elfutils/tests/hello.h"
 file[3] = "/opt/local/install/gcc/lib/gcc/x86_64-pc-linux-gnu/9.0.0/include/stddef.h"
off = 612
 dirs[0] = "/home/mark/src/elfutils/tests"
 dirs[1] = "/usr/include"
 file[0] = "???"
 file[1] = "/home/mark/src/elfutils/tests/world.c"
 file[2] = "/home/mark/src/elfutils/tests/hello.h"
 file[3] = "/usr/include/stdlib.h"
off = 0
 dirs[0] = "/home/mark/src/elfutils/tests"
 dirs[1] = "/opt/local/install/gcc/lib/gcc/x86_64-pc-linux-gnu/9.0.0/include"
 file[0] = "???"
 file[1] = "/home/mark/src/elfutils/tests/hello.c"
 file[2] = "/home/mark/src/elfutils/tests/hello.h"
 file[3] = "/opt/local/install/gcc/lib/gcc/x86_64-pc-linux-gnu/9.0.0/include/stddef.h"
off = 0
 dirs[0] = "/home/mark/src/elfutils/tests"
 dirs[1] = "/usr/include"
 file[0] = "???"
 file[1] = "/home/mark/src/elfutils/tests/world.c"
 file[2] = "/home/mark/src/elfutils/tests/hello.h"
 file[3] = "/usr/include/stdlib.h"
EOF

# No problem with dirs[0] for DWARF5 line tables.
testrun_compare ${abs_builddir}/next-files testfile-splitdwarf-5 testfile-hello5.dwo testfile-world5.dwo <<\EOF
off = 0
 dirs[0] = "/home/mark/src/elfutils/tests"
 dirs[1] = "/opt/local/install/gcc/lib/gcc/x86_64-pc-linux-gnu/9.0.0/include"
 file[0] = "/home/mark/src/elfutils/tests/hello.c"
 file[1] = "/home/mark/src/elfutils/tests/hello.c"
 file[2] = "/home/mark/src/elfutils/tests/hello.h"
 file[3] = "/opt/local/install/gcc/lib/gcc/x86_64-pc-linux-gnu/9.0.0/include/stddef.h"
off = 655
 dirs[0] = "/home/mark/src/elfutils/tests"
 dirs[1] = "/usr/include"
 file[0] = "/home/mark/src/elfutils/tests/world.c"
 file[1] = "/home/mark/src/elfutils/tests/world.c"
 file[2] = "/home/mark/src/elfutils/tests/hello.h"
 file[3] = "/usr/include/stdlib.h"
off = 0
 dirs[0] = "/home/mark/src/elfutils/tests"
 dirs[1] = "/opt/local/install/gcc/lib/gcc/x86_64-pc-linux-gnu/9.0.0/include"
 file[0] = "/home/mark/src/elfutils/tests/hello.c"
 file[1] = "/home/mark/src/elfutils/tests/hello.c"
 file[2] = "/home/mark/src/elfutils/tests/hello.h"
 file[3] = "/opt/local/install/gcc/lib/gcc/x86_64-pc-linux-gnu/9.0.0/include/stddef.h"
off = 0
 dirs[0] = "/home/mark/src/elfutils/tests"
 dirs[1] = "/usr/include"
 file[0] = "/home/mark/src/elfutils/tests/world.c"
 file[1] = "/home/mark/src/elfutils/tests/world.c"
 file[2] = "/home/mark/src/elfutils/tests/hello.h"
 file[3] = "/usr/include/stdlib.h"
EOF

# Created from testfile using
# cp testfile testfile-only-debug-line
# eu-strip -g --keep-section .debug_line
#
# Note how the comp dir cannot be retrieved and some files become relative.
testfiles testfile-only-debug-line
testrun_compare ${abs_builddir}/next-files testfile-only-debug-line <<\EOF
off = 0
 dirs[0] = (null)
 file[0] = "???"
 file[1] = "m.c"
off = 75
 dirs[0] = (null)
 file[0] = "???"
 file[1] = "b.c"
 file[2] = "/usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h"
 file[3] = "/usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h"
 file[4] = "/usr/include/bits/types.h"
 file[5] = "/usr/include/bits/sched.h"
 file[6] = "/usr/include/bits/pthreadtypes.h"
 file[7] = "/usr/include/stdio.h"
 file[8] = "/usr/include/libio.h"
 file[9] = "/usr/include/wchar.h"
 file[10] = "/usr/include/_G_config.h"
 file[11] = "/usr/include/gconv.h"
off = 480
 dirs[0] = (null)
 file[0] = "???"
 file[1] = "f.c"
EOF

# See testfile-dwp.source.
testfiles testfile-dwp-5 testfile-dwp-5.dwp
testfiles testfile-dwp-4 testfile-dwp-4.dwp

testrun_compare ${abs_builddir}/next-files testfile-dwp-5 << EOF
off = 0
 dirs[0] = "/home/osandov/src/elfutils/tests"
 dirs[1] = "/usr/include"
 file[0] = "/home/osandov/src/elfutils/tests/foo.cc"
 file[1] = "/home/osandov/src/elfutils/tests/foo.cc"
 file[2] = "/home/osandov/src/elfutils/tests/foobar.h"
 file[3] = "/usr/include/stdc-predef.h"
off = 383
 dirs[0] = "/home/osandov/src/elfutils/tests"
 dirs[1] = "/usr/include"
 file[0] = "/home/osandov/src/elfutils/tests/bar.cc"
 file[1] = "/home/osandov/src/elfutils/tests/bar.cc"
 file[2] = "/home/osandov/src/elfutils/tests/foobar.h"
 file[3] = "/usr/include/stdc-predef.h"
off = 487
 dirs[0] = "/home/osandov/src/elfutils/tests"
 dirs[1] = "/usr/include"
 file[0] = "/home/osandov/src/elfutils/tests/main.cc"
 file[1] = "/home/osandov/src/elfutils/tests/main.cc"
 file[2] = "/home/osandov/src/elfutils/tests/foobar.h"
 file[3] = "/usr/include/stdc-predef.h"
EOF

testrun_compare ${abs_builddir}/next-files testfile-dwp-5.dwp << EOF
off = 0
 dirs[0] = "/home/osandov/src/elfutils/tests"
 dirs[1] = "/usr/include"
 file[0] = "/home/osandov/src/elfutils/tests/foo.cc"
 file[1] = "/home/osandov/src/elfutils/tests/foo.cc"
 file[2] = "/home/osandov/src/elfutils/tests/foobar.h"
 file[3] = "/usr/include/stdc-predef.h"
off = 127
 dirs[0] = "/home/osandov/src/elfutils/tests"
 dirs[1] = "/usr/include"
 file[0] = "/home/osandov/src/elfutils/tests/bar.cc"
 file[1] = "/home/osandov/src/elfutils/tests/bar.cc"
 file[2] = "/home/osandov/src/elfutils/tests/foobar.h"
 file[3] = "/usr/include/stdc-predef.h"
off = 254
 dirs[0] = "/home/osandov/src/elfutils/tests"
 dirs[1] = "/usr/include"
 file[0] = "/home/osandov/src/elfutils/tests/main.cc"
 file[1] = "/home/osandov/src/elfutils/tests/main.cc"
 file[2] = "/home/osandov/src/elfutils/tests/foobar.h"
 file[3] = "/usr/include/stdc-predef.h"
EOF

testrun_compare ${abs_builddir}/next-files testfile-dwp-4 << EOF
off = 0
 dirs[0] = "/home/osandov/src/elfutils/tests"
 dirs[1] = "/usr/include"
 file[0] = "???"
 file[1] = "/home/osandov/src/elfutils/tests/foo.cc"
 file[2] = "/home/osandov/src/elfutils/tests/foobar.h"
 file[3] = "/usr/include/stdc-predef.h"
off = 397
 dirs[0] = "/home/osandov/src/elfutils/tests"
 dirs[1] = "/usr/include"
 file[0] = "???"
 file[1] = "/home/osandov/src/elfutils/tests/bar.cc"
 file[2] = "/home/osandov/src/elfutils/tests/foobar.h"
 file[3] = "/usr/include/stdc-predef.h"
off = 515
 dirs[0] = "/home/osandov/src/elfutils/tests"
 dirs[1] = "/usr/include"
 file[0] = "???"
 file[1] = "/home/osandov/src/elfutils/tests/main.cc"
 file[2] = "/home/osandov/src/elfutils/tests/foobar.h"
 file[3] = "/usr/include/stdc-predef.h"
EOF

testrun_compare ${abs_builddir}/next-files testfile-dwp-4.dwp << EOF
off = 0
 dirs[0] = "/home/osandov/src/elfutils/tests"
 dirs[1] = "/usr/include"
 file[0] = "???"
 file[1] = "/home/osandov/src/elfutils/tests/foo.cc"
 file[2] = "/home/osandov/src/elfutils/tests/foobar.h"
 file[3] = "/usr/include/stdc-predef.h"
off = 82
 dirs[0] = "/home/osandov/src/elfutils/tests"
 dirs[1] = "/usr/include"
 file[0] = "???"
 file[1] = "/home/osandov/src/elfutils/tests/bar.cc"
 file[2] = "/home/osandov/src/elfutils/tests/foobar.h"
 file[3] = "/usr/include/stdc-predef.h"
off = 164
 dirs[0] = "/home/osandov/src/elfutils/tests"
 dirs[1] = "/usr/include"
 file[0] = "???"
 file[1] = "/home/osandov/src/elfutils/tests/main.cc"
 file[2] = "/home/osandov/src/elfutils/tests/foobar.h"
 file[3] = "/usr/include/stdc-predef.h"
EOF

exit 0
