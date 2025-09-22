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

# See run-addr2line-test.sh run-addr2line-i-test.sh run-addr2line-i-lex-test.sh
# Output/files/lines matched should equal what is done through addr2line
# which uses dwfl_module_getsrc. This test uses dwarf_addrdie and
# dwarf_getsrc_die
testfiles testfile testfile-inlines testfile-lex-inlines
tempfiles testfile-no-aranges testfile-inlines-no-aranges getsrc_die.out
tempfiles testfile-lex-inlines-no-aranges good1.out good2.out good3.out

cat > good1.out <<\EOF
/home/drepper/gnu/new-bu/build/ttt/f.c:3
/home/drepper/gnu/new-bu/build/ttt/b.c:4
EOF

cat good1.out | testrun_compare ${abs_top_builddir}/tests/getsrc_die testfile 0x08048468 0x0804845c

cat > good2.out <<\EOF
/tmp/x.cpp:5
/tmp/x.cpp:6
/tmp/x.cpp:10
/tmp/x.cpp:11
/tmp/x.cpp:5
/tmp/x.cpp:10
/tmp/x.cpp:5
/tmp/x.cpp:10
/tmp/x.cpp:10
/tmp/x.cpp:5
EOF

cat good2.out | testrun_compare ${abs_top_builddir}/tests/getsrc_die testfile-inlines 0x00000000000005a0 0x00000000000005a1 0x00000000000005b0 0x00000000000005b1 0x00000000000005c0 0x00000000000005d0 0x00000000000005e0 0x00000000000005e1 0x00000000000005f1 0x00000000000005f2

cat > good3.out <<\EOF
/tmp/x.cpp:5
/tmp/x.cpp:5
/tmp/x.cpp:5
/tmp/x.cpp:5
EOF

cat good3.out | testrun_compare ${abs_top_builddir}/tests/getsrc_die testfile-lex-inlines 0x0000000000000680 0x0000000000000681 0x0000000000000690 0x0000000000000691

# Each test should also pass with no .debug_aranges present.  If objcopy cannot
# strip the testfiles then skip the remaining tests.
objcopy --remove-section .debug_aranges testfile testfile-no-aranges || exit 0
objcopy --remove-section .debug_aranges testfile-inlines testfile-inlines-no-aranges || exit 0
objcopy --remove-section .debug_aranges testfile-lex-inlines testfile-lex-inlines-no-aranges || exit 0

cat good1.out | testrun_compare ${abs_top_builddir}/tests/getsrc_die testfile-no-aranges 0x08048468 0x0804845c

cat good2.out | testrun_compare ${abs_top_builddir}/tests/getsrc_die testfile-inlines-no-aranges 0x00000000000005a0 0x00000000000005a1 0x00000000000005b0 0x00000000000005b1 0x00000000000005c0 0x00000000000005d0 0x00000000000005e0 0x00000000000005e1 0x00000000000005f1 0x00000000000005f2

cat good3.out | testrun_compare ${abs_top_builddir}/tests/getsrc_die testfile-lex-inlines-no-aranges 0x0000000000000680 0x0000000000000681 0x0000000000000690 0x0000000000000691

exit 0
