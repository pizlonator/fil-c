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

# Tests readelf --debug-dump=aranges and --debug-dump=decodedaranges
#
# - foobarbaz.h
#
# int bar ();
# int baz (int i);
#
# - bar.c
#
# #include "foobarbaz.h"
#
# static int bi;
#
# static int
# barbaz (int i)
# {
#   return i * 2 - 1;
# }
#
# __attribute__ ((constructor)) void
# nobar ()
# {
#   bi = 1;
# }
#
# int
# bar ()
# {
#   bi++;
#   return barbaz (bi);
# }
#
# - foo.c
#
# include "foobarbaz.h"
#
# static int fi = 0;
#
# static int
# foo (int i, int j)
# {
#   if (i > j)
#     return i - j + fi;
#   else
#     return (2 * j) - i + fi;
# }
#
# int
# main (int argc, char **argv)
# {
#   int a = bar ();
#   int b = baz (a + argc);
#   int r = foo (a, b) - 1;
#
#   return r - 48;
# }
#
# - baz.c
# include "foobarbaz.h"
#
# static int bj;
#
# static int
# bazbaz (int j)
# {
#   return bj * j - bar ();
# }
#
# __attribute__ ((constructor)) void
# nobaz ()
# {
#   bj = 1;
# }
#
# int
# baz (int i)
# {
#   if (i < 0)
#     return bazbaz (i);
#   else
#     {
#       while (i-- > 0)
#         bj += bar ();
#     }
#   return bazbaz (i);
# }
#
# gcc -g -O2 -m32 -c baz.c
# gcc -g -O2 -m32 -c bar.c
# gcc -g -O2 -m32 -c foo.c
# gcc -g -O2 -m32 -o testfilefoobarbaz foo.o bar.o baz.o

testfiles testfilefoobarbaz

testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=aranges testfilefoobarbaz <<EOF

DWARF section [27] '.debug_aranges' at offset 0x1044:

Table at offset 0:

 Length:            28
 DWARF version:      2
 CU offset:          0
 Address size:       4
 Segment size:       0

   0x080482f0 <main>..0x08048323 <main+0x33>

Table at offset 32:

 Length:            36
 DWARF version:      2
 CU offset:        136
 Address size:       4
 Segment size:       0

   0x08048440 <bar>..0x08048451 <bar+0x11>
   0x08048330 <nobar>..0x0804833a <nobar+0xa>

Table at offset 72:

 Length:            36
 DWARF version:      2
 CU offset:        1d1
 Address size:       4
 Segment size:       0

   0x08048460 <baz>..0x080484bb <baz+0x5b>
   0x08048340 <nobaz>..0x0804834a <nobaz+0xa>
EOF

testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=decodedaranges testfilefoobarbaz <<\EOF

DWARF section [27] '.debug_aranges' at offset 0x1044 contains 5 entries:
 [0] start: 0x080482f0, length:    52, CU DIE offset:     11
 [1] start: 0x08048330, length:    11, CU DIE offset:    321
 [2] start: 0x08048340, length:    11, CU DIE offset:    476
 [3] start: 0x08048440, length:    18, CU DIE offset:    321
 [4] start: 0x08048460, length:    92, CU DIE offset:    476
EOF

exit 0
