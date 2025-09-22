#! /bin/sh
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

# See run-dwarf-ranges.sh
# Compiled with:
# gcc -c -O2 -o testfile-ranges-hello.o -gsplit-dwarf -gdwarf-4 hello.c
# gcc -c -O2 -o testfile-ranges-world.o -gsplit-dwarf -gdwarf-4 world.c
# gcc -o testfilesplitranges4 -O2 \
#        testfile-ranges-hello.o testfile-ranges-world.o
# eu-strip -f testfilesplitranges4.debug testfilesplitranges4

testfiles testfilesplitranges4.debug
testfiles testfile-ranges-hello.dwo testfile-ranges-world.dwo

testrun_compare ${abs_builddir}/all-dwarf-ranges testfilesplitranges4.debug <<\EOF
die: hello.c (11)
 4004e0..4004ff
 4003e0..4003f7

die: no_say (2e)
 4004f0..4004ff

die: main (2e)
 4003e0..4003f7

die: subject (1d)
 4003e3..4003ef

die: subject (2e)
 4004e0..4004f0

die: world.c (11)
 400500..400567

die: no_main (2e)
 400550..400567

die: no_subject (1d)
 400553..40055f

die: say (2e)
 400500..400540

die: happy (1d)
 40051c..400526
 400530..400534
 400535..40053f

die: sad (1d)
 40051c..400526
 400535..40053f

die: no_subject (2e)
 400540..400550

EOF

# Same with -gdwarf-5
# gcc -c -O2 -o testfile-ranges-hello5.o -gsplit-dwarf -gdwarf-5 hello.c
# gcc -c -O2 -o testfile-ranges-world5.o -gsplit-dwarf -gdwarf-5 world.c
# gcc -o testfilesplitranges5 -O2 testfile-ranges-hello5.o testfile-ranges-world5.o
# eu-strip -f testfilesplitranges5.debug testfilesplitranges5

testfiles testfilesplitranges5.debug
testfiles testfile-ranges-hello5.dwo testfile-ranges-world5.dwo

testrun_compare ${abs_builddir}/all-dwarf-ranges testfilesplitranges5.debug <<\EOF
die: hello.c (11)
 401150..40117a
 401050..401067

die: no_say (2e)
 401160..40117a

die: main (2e)
 401050..401067

die: subject (1d)
 401053..40105f

die: subject (2e)
 401150..401160

die: world.c (11)
 401180..4011e7

die: no_main (2e)
 4011d0..4011e7

die: no_subject (1d)
 4011d3..4011df

die: say (2e)
 401180..4011c0

die: happy (1d)
 40119b..40119b
 40119c..4011a6
 4011b0..4011b4
 4011b5..4011bf

die: sad (1d)
 40119b..40119b
 40119c..4011a6
 4011b4..4011b4
 4011b5..4011bf

die: no_subject (2e)
 4011c0..4011d0

EOF

# See testfile-dwp.source.
testfiles testfile-dwp-5 testfile-dwp-5.dwp
testfiles testfile-dwp-4 testfile-dwp-4.dwp

testrun_compare ${abs_builddir}/all-dwarf-ranges testfile-dwp-5 << EOF
die: foo.cc (11)
 401190..401200

die: foo (2e)
 4011c0..401200

die: x_x (1d)
 4011cb..4011eb
 4011f8..401200

die: <unknown> (b)
 4011cb..4011eb
 4011f8..401200

die: x_x (2e)
 401190..4011bd

die: <unknown> (b)
 401190..401190
 401192..4011bb

die: bar.cc (11)
 401200..40121b

die: bar (2e)
 401200..40121b

die: main.cc (11)
 401020..4010a0

die: main (2e)
 401020..4010a0

die: fibonacci (1d)
 401030..401032
 401036..401060
 401099..4010a0

die: fibonacci (1d)
 40103a..401060
 401099..4010a0

die: <unknown> (b)
 40103a..401060
 401099..4010a0

die: <unknown> (b)
 40103a..401044
 401050..401060

die: <unknown> (b)
 401050..401053
 401056..401059

EOF

testrun_compare ${abs_builddir}/all-dwarf-ranges testfile-dwp-4 << EOF
die: foo.cc (11)
 401190..401200

die: foo (2e)
 4011c0..401200

die: x_x (1d)
 4011cb..4011eb
 4011f8..401200

die: <unknown> (b)
 4011cb..4011eb
 4011f8..401200

die: x_x (2e)
 401190..4011bd

die: bar.cc (11)
 401200..40121b

die: bar (2e)
 401200..40121b

die: main.cc (11)
 401020..4010a0

die: main (2e)
 401020..4010a0

die: fibonacci (1d)
 401030..401032
 401036..401060
 401099..4010a0

die: fibonacci (1d)
 40103a..401060
 401099..4010a0

die: <unknown> (b)
 40103a..401060
 401099..4010a0

die: <unknown> (b)
 40103a..401044
 401050..401060

die: <unknown> (b)
 401050..401053
 401056..401059

EOF

exit 0
