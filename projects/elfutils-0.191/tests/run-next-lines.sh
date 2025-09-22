#! /bin/sh
# Variant of run-get-lines that uses dwarf_next_lines.
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

testfiles testfile testfile2 testfilenolines

testrun_compare ${abs_builddir}/next-lines testfile testfile2 <<\EOF
off = 0
 5 lines
804842c: /home/drepper/gnu/new-bu/build/ttt/m.c:5:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
8048432: /home/drepper/gnu/new-bu/build/ttt/m.c:6:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
804844d: /home/drepper/gnu/new-bu/build/ttt/m.c:7:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
8048458: /home/drepper/gnu/new-bu/build/ttt/m.c:8:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
804845a: /home/drepper/gnu/new-bu/build/ttt/m.c:8:0: is_stmt:yes, end_seq:yes, bb:no, prologue:no, epilogue:no
off = 75
 4 lines
804845c: /home/drepper/gnu/new-bu/build/ttt/b.c:4:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
804845f: /home/drepper/gnu/new-bu/build/ttt/b.c:5:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
8048464: /home/drepper/gnu/new-bu/build/ttt/b.c:6:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
8048466: /home/drepper/gnu/new-bu/build/ttt/b.c:6:0: is_stmt:yes, end_seq:yes, bb:no, prologue:no, epilogue:no
off = 480
 4 lines
8048468: /home/drepper/gnu/new-bu/build/ttt/f.c:3:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
804846b: /home/drepper/gnu/new-bu/build/ttt/f.c:4:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
8048470: /home/drepper/gnu/new-bu/build/ttt/f.c:5:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
8048472: /home/drepper/gnu/new-bu/build/ttt/f.c:5:0: is_stmt:yes, end_seq:yes, bb:no, prologue:no, epilogue:no
off = 0
 4 lines
10000470: /shoggoth/drepper/b.c:4:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
1000047c: /shoggoth/drepper/b.c:5:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
10000480: /shoggoth/drepper/b.c:6:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
10000490: /shoggoth/drepper/b.c:6:0: is_stmt:yes, end_seq:yes, bb:no, prologue:no, epilogue:no
off = 418
 4 lines
10000490: /shoggoth/drepper/f.c:3:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
1000049c: /shoggoth/drepper/f.c:4:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
100004a0: /shoggoth/drepper/f.c:5:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
100004b0: /shoggoth/drepper/f.c:5:0: is_stmt:yes, end_seq:yes, bb:no, prologue:no, epilogue:no
off = 485
 5 lines
100004b0: /shoggoth/drepper/m.c:5:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
100004cc: /shoggoth/drepper/m.c:6:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
100004e8: /shoggoth/drepper/m.c:7:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
100004f4: /shoggoth/drepper/m.c:8:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
10000514: /shoggoth/drepper/m.c:8:0: is_stmt:yes, end_seq:yes, bb:no, prologue:no, epilogue:no
EOF

# - lines.c
# int ft;
#
# int
# main (int argc, char **argv)
# {
#   return ft - 42;
# }
#
# - nolines.c
# int ft = 42;
#
# gcc -g -c lines.c
# gcc -g -c nolines.c
# gcc -g -o testfilenolines lines.o nolines.o

testrun_compare ${abs_builddir}/next-lines testfilenolines <<\EOF
off = 0
 4 lines
400474: /home/mark/src/tests/lines.c:5:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
40047f: /home/mark/src/tests/lines.c:6:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
400488: /home/mark/src/tests/lines.c:7:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
40048a: /home/mark/src/tests/lines.c:7:0: is_stmt:yes, end_seq:yes, bb:no, prologue:no, epilogue:no
off = 59
 0 lines
EOF

# See run-next-files.
# Note no, comp_dir, so all paths are relative.
testfiles testfile-only-debug-line
testrun_compare ${abs_builddir}/next-lines testfile-only-debug-line <<\EOF
off = 0
 5 lines
804842c: m.c:5:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
8048432: m.c:6:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
804844d: m.c:7:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
8048458: m.c:8:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
804845a: m.c:8:0: is_stmt:yes, end_seq:yes, bb:no, prologue:no, epilogue:no
off = 75
 4 lines
804845c: b.c:4:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
804845f: b.c:5:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
8048464: b.c:6:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
8048466: b.c:6:0: is_stmt:yes, end_seq:yes, bb:no, prologue:no, epilogue:no
off = 480
 4 lines
8048468: f.c:3:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
804846b: f.c:4:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
8048470: f.c:5:0: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
8048472: f.c:5:0: is_stmt:yes, end_seq:yes, bb:no, prologue:no, epilogue:no
EOF

testfiles testfile-dwp-5 testfile-dwp-5.dwp
testrun_compare ${abs_builddir}/next-lines testfile-dwp-5 << EOF
off = 0
 63 lines
401190: /home/osandov/src/elfutils/tests/foo.cc:7:1: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401190: /home/osandov/src/elfutils/tests/foo.cc:8:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401190: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401190: /home/osandov/src/elfutils/tests/foo.cc:7:1: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401192: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401196: /home/osandov/src/elfutils/tests/foo.cc:8:12: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
40119e: /home/osandov/src/elfutils/tests/foo.cc:9:5: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
40119e: /home/osandov/src/elfutils/tests/foo.cc:9:7: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011a1: /home/osandov/src/elfutils/tests/foo.cc:8:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011a1: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011a4: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011b0: /home/osandov/src/elfutils/tests/foo.cc:9:5: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011b0: /home/osandov/src/elfutils/tests/foo.cc:9:7: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011b3: /home/osandov/src/elfutils/tests/foo.cc:8:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011b3: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011b3: /home/osandov/src/elfutils/tests/foo.cc:9:5: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011b3: /home/osandov/src/elfutils/tests/foo.cc:9:7: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011b6: /home/osandov/src/elfutils/tests/foo.cc:8:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011b6: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011b9: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011bb: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011bb: /home/osandov/src/elfutils/tests/foo.cc:10:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011bb: /home/osandov/src/elfutils/tests/foo.cc:11:1: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011bc: /home/osandov/src/elfutils/tests/foo.cc:11:1: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011c0: /home/osandov/src/elfutils/tests/foo.cc:15:1: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011c0: /home/osandov/src/elfutils/tests/foo.cc:16:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011c0: /home/osandov/src/elfutils/tests/foo.cc:16:7: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011c2: /home/osandov/src/elfutils/tests/foo.cc:17:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011c2: /home/osandov/src/elfutils/tests/foo.cc:17:11: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011c5: /home/osandov/src/elfutils/tests/foo.cc:17:3: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011c9: /home/osandov/src/elfutils/tests/foo.cc:18:5: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011c9: /home/osandov/src/elfutils/tests/foo.cc:18:7: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011cb: /home/osandov/src/elfutils/tests/foo.cc:19:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011cb: /home/osandov/src/elfutils/tests/foo.cc:6:1: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011cb: /home/osandov/src/elfutils/tests/foo.cc:8:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011cb: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011cb: /home/osandov/src/elfutils/tests/foo.cc:8:12: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011cd: /home/osandov/src/elfutils/tests/foo.cc:8:12: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011d1: /home/osandov/src/elfutils/tests/foo.cc:9:5: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011d1: /home/osandov/src/elfutils/tests/foo.cc:9:7: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011d4: /home/osandov/src/elfutils/tests/foo.cc:8:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011d4: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011d7: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011e0: /home/osandov/src/elfutils/tests/foo.cc:9:5: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011e0: /home/osandov/src/elfutils/tests/foo.cc:9:7: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011e3: /home/osandov/src/elfutils/tests/foo.cc:8:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011e3: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011e3: /home/osandov/src/elfutils/tests/foo.cc:9:5: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011e3: /home/osandov/src/elfutils/tests/foo.cc:9:7: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011e6: /home/osandov/src/elfutils/tests/foo.cc:8:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011e6: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011e9: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011eb: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011eb: /home/osandov/src/elfutils/tests/foo.cc:10:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011eb: /home/osandov/src/elfutils/tests/foo.cc:10:3: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011eb: /home/osandov/src/elfutils/tests/foo.cc:19:10: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011f0: /home/osandov/src/elfutils/tests/foo.cc:20:1: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011f8: /home/osandov/src/elfutils/tests/foo.cc:19:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011f8: /home/osandov/src/elfutils/tests/foo.cc:6:1: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011f8: /home/osandov/src/elfutils/tests/foo.cc:8:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011f8: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011fc: /home/osandov/src/elfutils/tests/foo.cc:8:12: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401200: /home/osandov/src/elfutils/tests/foo.cc:8:12: is_stmt:no, end_seq:yes, bb:no, prologue:no, epilogue:no
off = 383
 7 lines
401200: /home/osandov/src/elfutils/tests/bar.cc:7:1: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401200: /home/osandov/src/elfutils/tests/bar.cc:8:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401200: /home/osandov/src/elfutils/tests/bar.cc:8:12: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401204: /home/osandov/src/elfutils/tests/bar.cc:8:7: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401207: /home/osandov/src/elfutils/tests/bar.cc:11:18: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
40121a: /home/osandov/src/elfutils/tests/bar.cc:12:1: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
40121b: /home/osandov/src/elfutils/tests/bar.cc:12:1: is_stmt:no, end_seq:yes, bb:no, prologue:no, epilogue:no
off = 487
 41 lines
401020: /home/osandov/src/elfutils/tests/main.cc:7:1: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401020: /home/osandov/src/elfutils/tests/main.cc:8:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401020: /home/osandov/src/elfutils/tests/main.cc:7:1: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401021: /home/osandov/src/elfutils/tests/main.cc:8:28: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401028: /home/osandov/src/elfutils/tests/main.cc:7:1: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
40102c: /home/osandov/src/elfutils/tests/main.cc:8:28: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401030: /home/osandov/src/elfutils/tests/main.cc:9:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401030: /home/osandov/src/elfutils/tests/foobar.h:17:1: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401030: /home/osandov/src/elfutils/tests/foobar.h:19:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401030: /home/osandov/src/elfutils/tests/foobar.h:20:12: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401032: /home/osandov/src/elfutils/tests/main.cc:8:40: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401036: /home/osandov/src/elfutils/tests/foobar.h:19:3: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
40103a: /home/osandov/src/elfutils/tests/foobar.h:19:3: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
40103a: /home/osandov/src/elfutils/tests/foobar.h:25:34: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
40103f: /home/osandov/src/elfutils/tests/foobar.h:25:25: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401044: /home/osandov/src/elfutils/tests/foobar.h:24:12: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401049: /home/osandov/src/elfutils/tests/foobar.h:23:12: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401050: /home/osandov/src/elfutils/tests/foobar.h:27:4: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401053: /home/osandov/src/elfutils/tests/foobar.h:25:7: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401056: /home/osandov/src/elfutils/tests/foobar.h:27:9: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401059: /home/osandov/src/elfutils/tests/foobar.h:28:4: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401059: /home/osandov/src/elfutils/tests/foobar.h:29:4: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401059: /home/osandov/src/elfutils/tests/foobar.h:29:4: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401059: /home/osandov/src/elfutils/tests/foobar.h:25:7: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401059: /home/osandov/src/elfutils/tests/foobar.h:25:34: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401060: /home/osandov/src/elfutils/tests/foobar.h:25:34: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401060: /home/osandov/src/elfutils/tests/main.cc:9:51: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401065: /home/osandov/src/elfutils/tests/main.cc:9:40: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401068: /home/osandov/src/elfutils/tests/main.cc:9:40: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401076: /home/osandov/src/elfutils/tests/main.cc:9:51: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
40107b: /home/osandov/src/elfutils/tests/main.cc:10:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
40107b: /home/osandov/src/elfutils/tests/main.cc:10:19: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401080: /home/osandov/src/elfutils/tests/main.cc:10:19: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401085: /home/osandov/src/elfutils/tests/main.cc:10:33: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
40108a: /home/osandov/src/elfutils/tests/main.cc:10:19: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
40108c: /home/osandov/src/elfutils/tests/main.cc:10:33: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401091: /home/osandov/src/elfutils/tests/main.cc:11:1: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401095: /home/osandov/src/elfutils/tests/main.cc:10:22: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401097: /home/osandov/src/elfutils/tests/main.cc:11:1: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401099: /home/osandov/src/elfutils/tests/foobar.h:24:12: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4010a0: /home/osandov/src/elfutils/tests/foobar.h:24:12: is_stmt:no, end_seq:yes, bb:no, prologue:no, epilogue:no
EOF

testfiles testfile-dwp-4 testfile-dwp-4.dwp
testrun_compare ${abs_builddir}/next-lines testfile-dwp-4 << EOF
off = 0
 63 lines
401190: /home/osandov/src/elfutils/tests/foo.cc:7:1: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401190: /home/osandov/src/elfutils/tests/foo.cc:8:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401190: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401190: /home/osandov/src/elfutils/tests/foo.cc:7:1: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401192: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401196: /home/osandov/src/elfutils/tests/foo.cc:8:12: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
40119e: /home/osandov/src/elfutils/tests/foo.cc:9:5: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
40119e: /home/osandov/src/elfutils/tests/foo.cc:9:7: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011a1: /home/osandov/src/elfutils/tests/foo.cc:8:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011a1: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011a4: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011b0: /home/osandov/src/elfutils/tests/foo.cc:9:5: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011b0: /home/osandov/src/elfutils/tests/foo.cc:9:7: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011b3: /home/osandov/src/elfutils/tests/foo.cc:8:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011b3: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011b3: /home/osandov/src/elfutils/tests/foo.cc:9:5: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011b3: /home/osandov/src/elfutils/tests/foo.cc:9:7: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011b6: /home/osandov/src/elfutils/tests/foo.cc:8:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011b6: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011b9: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011bb: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011bb: /home/osandov/src/elfutils/tests/foo.cc:10:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011bb: /home/osandov/src/elfutils/tests/foo.cc:11:1: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011bc: /home/osandov/src/elfutils/tests/foo.cc:11:1: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011c0: /home/osandov/src/elfutils/tests/foo.cc:15:1: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011c0: /home/osandov/src/elfutils/tests/foo.cc:16:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011c0: /home/osandov/src/elfutils/tests/foo.cc:16:7: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011c2: /home/osandov/src/elfutils/tests/foo.cc:17:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011c2: /home/osandov/src/elfutils/tests/foo.cc:17:11: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011c5: /home/osandov/src/elfutils/tests/foo.cc:17:3: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011c9: /home/osandov/src/elfutils/tests/foo.cc:18:5: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011c9: /home/osandov/src/elfutils/tests/foo.cc:18:7: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011cb: /home/osandov/src/elfutils/tests/foo.cc:19:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011cb: /home/osandov/src/elfutils/tests/foo.cc:6:1: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011cb: /home/osandov/src/elfutils/tests/foo.cc:8:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011cb: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011cb: /home/osandov/src/elfutils/tests/foo.cc:8:12: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011cd: /home/osandov/src/elfutils/tests/foo.cc:8:12: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011d1: /home/osandov/src/elfutils/tests/foo.cc:9:5: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011d1: /home/osandov/src/elfutils/tests/foo.cc:9:7: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011d4: /home/osandov/src/elfutils/tests/foo.cc:8:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011d4: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011d7: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011e0: /home/osandov/src/elfutils/tests/foo.cc:9:5: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011e0: /home/osandov/src/elfutils/tests/foo.cc:9:7: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011e3: /home/osandov/src/elfutils/tests/foo.cc:8:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011e3: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011e3: /home/osandov/src/elfutils/tests/foo.cc:9:5: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011e3: /home/osandov/src/elfutils/tests/foo.cc:9:7: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011e6: /home/osandov/src/elfutils/tests/foo.cc:8:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011e6: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011e9: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011eb: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011eb: /home/osandov/src/elfutils/tests/foo.cc:10:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011eb: /home/osandov/src/elfutils/tests/foo.cc:10:3: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011eb: /home/osandov/src/elfutils/tests/foo.cc:19:10: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011f0: /home/osandov/src/elfutils/tests/foo.cc:20:1: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4011f8: /home/osandov/src/elfutils/tests/foo.cc:19:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011f8: /home/osandov/src/elfutils/tests/foo.cc:6:1: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011f8: /home/osandov/src/elfutils/tests/foo.cc:8:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011f8: /home/osandov/src/elfutils/tests/foo.cc:8:21: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
4011fc: /home/osandov/src/elfutils/tests/foo.cc:8:12: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401200: /home/osandov/src/elfutils/tests/foo.cc:8:12: is_stmt:no, end_seq:yes, bb:no, prologue:no, epilogue:no
off = 397
 7 lines
401200: /home/osandov/src/elfutils/tests/bar.cc:7:1: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401200: /home/osandov/src/elfutils/tests/bar.cc:8:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401200: /home/osandov/src/elfutils/tests/bar.cc:8:12: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401204: /home/osandov/src/elfutils/tests/bar.cc:8:7: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401207: /home/osandov/src/elfutils/tests/bar.cc:11:18: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
40121a: /home/osandov/src/elfutils/tests/bar.cc:12:1: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
40121b: /home/osandov/src/elfutils/tests/bar.cc:12:1: is_stmt:no, end_seq:yes, bb:no, prologue:no, epilogue:no
off = 515
 41 lines
401020: /home/osandov/src/elfutils/tests/main.cc:7:1: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401020: /home/osandov/src/elfutils/tests/main.cc:8:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401020: /home/osandov/src/elfutils/tests/main.cc:7:1: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401021: /home/osandov/src/elfutils/tests/main.cc:8:28: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401028: /home/osandov/src/elfutils/tests/main.cc:7:1: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
40102c: /home/osandov/src/elfutils/tests/main.cc:8:28: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401030: /home/osandov/src/elfutils/tests/main.cc:9:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401030: /home/osandov/src/elfutils/tests/foobar.h:17:1: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401030: /home/osandov/src/elfutils/tests/foobar.h:19:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401030: /home/osandov/src/elfutils/tests/foobar.h:20:12: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401032: /home/osandov/src/elfutils/tests/main.cc:8:40: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401036: /home/osandov/src/elfutils/tests/foobar.h:19:3: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
40103a: /home/osandov/src/elfutils/tests/foobar.h:19:3: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
40103a: /home/osandov/src/elfutils/tests/foobar.h:25:34: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
40103f: /home/osandov/src/elfutils/tests/foobar.h:25:25: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401044: /home/osandov/src/elfutils/tests/foobar.h:24:12: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401049: /home/osandov/src/elfutils/tests/foobar.h:23:12: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401050: /home/osandov/src/elfutils/tests/foobar.h:27:4: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401053: /home/osandov/src/elfutils/tests/foobar.h:25:7: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401056: /home/osandov/src/elfutils/tests/foobar.h:27:9: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401059: /home/osandov/src/elfutils/tests/foobar.h:28:4: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401059: /home/osandov/src/elfutils/tests/foobar.h:29:4: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401059: /home/osandov/src/elfutils/tests/foobar.h:29:4: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401059: /home/osandov/src/elfutils/tests/foobar.h:25:7: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401059: /home/osandov/src/elfutils/tests/foobar.h:25:34: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
401060: /home/osandov/src/elfutils/tests/foobar.h:25:34: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401060: /home/osandov/src/elfutils/tests/main.cc:9:51: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401065: /home/osandov/src/elfutils/tests/main.cc:9:40: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401068: /home/osandov/src/elfutils/tests/main.cc:9:40: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401076: /home/osandov/src/elfutils/tests/main.cc:9:51: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
40107b: /home/osandov/src/elfutils/tests/main.cc:10:3: is_stmt:yes, end_seq:no, bb:no, prologue:no, epilogue:no
40107b: /home/osandov/src/elfutils/tests/main.cc:10:19: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401080: /home/osandov/src/elfutils/tests/main.cc:10:19: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401085: /home/osandov/src/elfutils/tests/main.cc:10:33: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
40108a: /home/osandov/src/elfutils/tests/main.cc:10:19: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
40108c: /home/osandov/src/elfutils/tests/main.cc:10:33: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401091: /home/osandov/src/elfutils/tests/main.cc:11:1: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401095: /home/osandov/src/elfutils/tests/main.cc:10:22: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401097: /home/osandov/src/elfutils/tests/main.cc:11:1: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
401099: /home/osandov/src/elfutils/tests/foobar.h:24:12: is_stmt:no, end_seq:no, bb:no, prologue:no, epilogue:no
4010a0: /home/osandov/src/elfutils/tests/foobar.h:24:12: is_stmt:no, end_seq:yes, bb:no, prologue:no, epilogue:no
EOF

exit 0
