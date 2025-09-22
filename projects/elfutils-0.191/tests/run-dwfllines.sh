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

testfiles testfile testfile2

testrun_compare ${abs_builddir}/dwfllines -e testfile <<\EOF
mod:  CU: [b] m.c
0 0x804842c /home/drepper/gnu/new-bu/build/ttt/m.c:5:0
 time: 0, len: 0, idx: 0, b: 1, e: 0, pe: 0, eb: 0, block: 0, isa: 0, disc: 0
1 0x8048432 /home/drepper/gnu/new-bu/build/ttt/m.c:6:0
 time: 0, len: 0, idx: 0, b: 1, e: 0, pe: 0, eb: 0, block: 0, isa: 0, disc: 0
2 0x804844d /home/drepper/gnu/new-bu/build/ttt/m.c:7:0
 time: 0, len: 0, idx: 0, b: 1, e: 0, pe: 0, eb: 0, block: 0, isa: 0, disc: 0
3 0x8048458 /home/drepper/gnu/new-bu/build/ttt/m.c:8:0
 time: 0, len: 0, idx: 0, b: 1, e: 0, pe: 0, eb: 0, block: 0, isa: 0, disc: 0
4 0x804845a /home/drepper/gnu/new-bu/build/ttt/m.c:8:0
 time: 0, len: 0, idx: 0, b: 1, e: 1, pe: 0, eb: 0, block: 0, isa: 0, disc: 0
mod:  CU: [ca] b.c
0 0x804845c /home/drepper/gnu/new-bu/build/ttt/b.c:4:0
 time: 0, len: 0, idx: 0, b: 1, e: 0, pe: 0, eb: 0, block: 0, isa: 0, disc: 0
1 0x804845f /home/drepper/gnu/new-bu/build/ttt/b.c:5:0
 time: 0, len: 0, idx: 0, b: 1, e: 0, pe: 0, eb: 0, block: 0, isa: 0, disc: 0
2 0x8048464 /home/drepper/gnu/new-bu/build/ttt/b.c:6:0
 time: 0, len: 0, idx: 0, b: 1, e: 0, pe: 0, eb: 0, block: 0, isa: 0, disc: 0
3 0x8048466 /home/drepper/gnu/new-bu/build/ttt/b.c:6:0
 time: 0, len: 0, idx: 0, b: 1, e: 1, pe: 0, eb: 0, block: 0, isa: 0, disc: 0
mod:  CU: [15fc] f.c
0 0x8048468 /home/drepper/gnu/new-bu/build/ttt/f.c:3:0
 time: 0, len: 0, idx: 0, b: 1, e: 0, pe: 0, eb: 0, block: 0, isa: 0, disc: 0
1 0x804846b /home/drepper/gnu/new-bu/build/ttt/f.c:4:0
 time: 0, len: 0, idx: 0, b: 1, e: 0, pe: 0, eb: 0, block: 0, isa: 0, disc: 0
2 0x8048470 /home/drepper/gnu/new-bu/build/ttt/f.c:5:0
 time: 0, len: 0, idx: 0, b: 1, e: 0, pe: 0, eb: 0, block: 0, isa: 0, disc: 0
3 0x8048472 /home/drepper/gnu/new-bu/build/ttt/f.c:5:0
 time: 0, len: 0, idx: 0, b: 1, e: 1, pe: 0, eb: 0, block: 0, isa: 0, disc: 0
EOF

testrun_compare ${abs_builddir}/dwfllines -e testfile2 <<\EOF
mod:  CU: [b] b.c
0 0x10000470 /shoggoth/drepper/b.c:4:0
 time: 0, len: 0, idx: 0, b: 1, e: 0, pe: 0, eb: 0, block: 0, isa: 0, disc: 0
1 0x1000047c /shoggoth/drepper/b.c:5:0
 time: 0, len: 0, idx: 0, b: 1, e: 0, pe: 0, eb: 0, block: 0, isa: 0, disc: 0
2 0x10000480 /shoggoth/drepper/b.c:6:0
 time: 0, len: 0, idx: 0, b: 1, e: 0, pe: 0, eb: 0, block: 0, isa: 0, disc: 0
3 0x10000490 /shoggoth/drepper/b.c:6:0
 time: 0, len: 0, idx: 0, b: 1, e: 1, pe: 0, eb: 0, block: 0, isa: 0, disc: 0
mod:  CU: [97d] f.c
0 0x10000490 /shoggoth/drepper/f.c:3:0
 time: 0, len: 0, idx: 0, b: 1, e: 0, pe: 0, eb: 0, block: 0, isa: 0, disc: 0
1 0x1000049c /shoggoth/drepper/f.c:4:0
 time: 0, len: 0, idx: 0, b: 1, e: 0, pe: 0, eb: 0, block: 0, isa: 0, disc: 0
2 0x100004a0 /shoggoth/drepper/f.c:5:0
 time: 0, len: 0, idx: 0, b: 1, e: 0, pe: 0, eb: 0, block: 0, isa: 0, disc: 0
3 0x100004b0 /shoggoth/drepper/f.c:5:0
 time: 0, len: 0, idx: 0, b: 1, e: 1, pe: 0, eb: 0, block: 0, isa: 0, disc: 0
mod:  CU: [9e4] m.c
0 0x100004b0 /shoggoth/drepper/m.c:5:0
 time: 0, len: 0, idx: 0, b: 1, e: 0, pe: 0, eb: 0, block: 0, isa: 0, disc: 0
1 0x100004cc /shoggoth/drepper/m.c:6:0
 time: 0, len: 0, idx: 0, b: 1, e: 0, pe: 0, eb: 0, block: 0, isa: 0, disc: 0
2 0x100004e8 /shoggoth/drepper/m.c:7:0
 time: 0, len: 0, idx: 0, b: 1, e: 0, pe: 0, eb: 0, block: 0, isa: 0, disc: 0
3 0x100004f4 /shoggoth/drepper/m.c:8:0
 time: 0, len: 0, idx: 0, b: 1, e: 0, pe: 0, eb: 0, block: 0, isa: 0, disc: 0
4 0x10000514 /shoggoth/drepper/m.c:8:0
 time: 0, len: 0, idx: 0, b: 1, e: 1, pe: 0, eb: 0, block: 0, isa: 0, disc: 0
EOF

testrun_on_self_quiet ${abs_builddir}/dwfllines -e

exit 0
