#! /bin/sh
# Copyright (C) 2015 Red Hat, Inc.
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

# See run-elfgetchdr.sh for testfiles.

testfiles testfile-zgnu64
testrun_compare ${abs_top_builddir}/tests/elfgetzdata read testfile-zgnu64 <<\EOF
1: .text, NOT compressed
2: .zdebug_aranges, GNU compressed, size: 60
3: .zdebug_info, GNU compressed, size: aa
4: .debug_abbrev, NOT compressed
5: .zdebug_line, GNU compressed, size: 8d
6: .shstrtab, NOT compressed
7: .symtab, NOT compressed
8: .strtab, NOT compressed
EOF

testrun_compare ${abs_top_builddir}/tests/elfgetzdata mmap testfile-zgnu64 <<\EOF
1: .text, NOT compressed
2: .zdebug_aranges, GNU compressed, size: 60
3: .zdebug_info, GNU compressed, size: aa
4: .debug_abbrev, NOT compressed
5: .zdebug_line, GNU compressed, size: 8d
6: .shstrtab, NOT compressed
7: .symtab, NOT compressed
8: .strtab, NOT compressed
EOF

testrun_compare ${abs_top_builddir}/tests/elfgetzdata mem testfile-zgnu64 <<\EOF
1: .text, NOT compressed
2: .zdebug_aranges, GNU compressed, size: 60
3: .zdebug_info, GNU compressed, size: aa
4: .debug_abbrev, NOT compressed
5: .zdebug_line, GNU compressed, size: 8d
6: .shstrtab, NOT compressed
7: .symtab, NOT compressed
8: .strtab, NOT compressed
EOF

testfiles testfile-zgnu64be
testrun_compare ${abs_top_builddir}/tests/elfgetzdata read testfile-zgnu64be <<\EOF
1: .text, NOT compressed
2: .eh_frame, NOT compressed
3: .zdebug_aranges, GNU compressed, size: 60
4: .zdebug_info, GNU compressed, size: 7e
5: .debug_abbrev, NOT compressed
6: .zdebug_line, GNU compressed, size: 8d
7: .shstrtab, NOT compressed
8: .symtab, NOT compressed
9: .strtab, NOT compressed
EOF

testrun_compare ${abs_top_builddir}/tests/elfgetzdata mmap testfile-zgnu64be <<\EOF
1: .text, NOT compressed
2: .eh_frame, NOT compressed
3: .zdebug_aranges, GNU compressed, size: 60
4: .zdebug_info, GNU compressed, size: 7e
5: .debug_abbrev, NOT compressed
6: .zdebug_line, GNU compressed, size: 8d
7: .shstrtab, NOT compressed
8: .symtab, NOT compressed
9: .strtab, NOT compressed
EOF

testrun_compare ${abs_top_builddir}/tests/elfgetzdata mem testfile-zgnu64be <<\EOF
1: .text, NOT compressed
2: .eh_frame, NOT compressed
3: .zdebug_aranges, GNU compressed, size: 60
4: .zdebug_info, GNU compressed, size: 7e
5: .debug_abbrev, NOT compressed
6: .zdebug_line, GNU compressed, size: 8d
7: .shstrtab, NOT compressed
8: .symtab, NOT compressed
9: .strtab, NOT compressed
EOF

testfiles testfile-zgabi64
testrun_compare ${abs_top_builddir}/tests/elfgetzdata read testfile-zgabi64 <<\EOF
1: .text, NOT compressed
2: .debug_aranges, ELF compressed, size: 60
3: .debug_info, ELF compressed, size: aa
4: .debug_abbrev, NOT compressed
5: .debug_line, ELF compressed, size: 8d
6: .shstrtab, NOT compressed
7: .symtab, NOT compressed
8: .strtab, NOT compressed
EOF

testrun_compare ${abs_top_builddir}/tests/elfgetzdata mmap testfile-zgabi64 <<\EOF
1: .text, NOT compressed
2: .debug_aranges, ELF compressed, size: 60
3: .debug_info, ELF compressed, size: aa
4: .debug_abbrev, NOT compressed
5: .debug_line, ELF compressed, size: 8d
6: .shstrtab, NOT compressed
7: .symtab, NOT compressed
8: .strtab, NOT compressed
EOF

testrun_compare ${abs_top_builddir}/tests/elfgetzdata mem testfile-zgabi64 <<\EOF
1: .text, NOT compressed
2: .debug_aranges, ELF compressed, size: 60
3: .debug_info, ELF compressed, size: aa
4: .debug_abbrev, NOT compressed
5: .debug_line, ELF compressed, size: 8d
6: .shstrtab, NOT compressed
7: .symtab, NOT compressed
8: .strtab, NOT compressed
EOF

testfiles testfile-zgabi64be
testrun_compare ${abs_top_builddir}/tests/elfgetzdata read testfile-zgabi64be <<\EOF
1: .text, NOT compressed
2: .eh_frame, NOT compressed
3: .debug_aranges, ELF compressed, size: 60
4: .debug_info, ELF compressed, size: 7e
5: .debug_abbrev, NOT compressed
6: .debug_line, ELF compressed, size: 8d
7: .shstrtab, NOT compressed
8: .symtab, NOT compressed
9: .strtab, NOT compressed
EOF

testrun_compare ${abs_top_builddir}/tests/elfgetzdata mmap testfile-zgabi64be <<\EOF
1: .text, NOT compressed
2: .eh_frame, NOT compressed
3: .debug_aranges, ELF compressed, size: 60
4: .debug_info, ELF compressed, size: 7e
5: .debug_abbrev, NOT compressed
6: .debug_line, ELF compressed, size: 8d
7: .shstrtab, NOT compressed
8: .symtab, NOT compressed
9: .strtab, NOT compressed
EOF

testrun_compare ${abs_top_builddir}/tests/elfgetzdata mem testfile-zgabi64be <<\EOF
1: .text, NOT compressed
2: .eh_frame, NOT compressed
3: .debug_aranges, ELF compressed, size: 60
4: .debug_info, ELF compressed, size: 7e
5: .debug_abbrev, NOT compressed
6: .debug_line, ELF compressed, size: 8d
7: .shstrtab, NOT compressed
8: .symtab, NOT compressed
9: .strtab, NOT compressed
EOF

testfiles testfile-zgnu32
testrun_compare ${abs_top_builddir}/tests/elfgetzdata read testfile-zgnu32 <<\EOF
1: .text, NOT compressed
2: .zdebug_aranges, GNU compressed, size: 40
3: .zdebug_info, GNU compressed, size: 9a
4: .debug_abbrev, NOT compressed
5: .zdebug_line, GNU compressed, size: 85
6: .shstrtab, NOT compressed
7: .symtab, NOT compressed
8: .strtab, NOT compressed
EOF

testrun_compare ${abs_top_builddir}/tests/elfgetzdata mmap testfile-zgnu32 <<\EOF
1: .text, NOT compressed
2: .zdebug_aranges, GNU compressed, size: 40
3: .zdebug_info, GNU compressed, size: 9a
4: .debug_abbrev, NOT compressed
5: .zdebug_line, GNU compressed, size: 85
6: .shstrtab, NOT compressed
7: .symtab, NOT compressed
8: .strtab, NOT compressed
EOF

testrun_compare ${abs_top_builddir}/tests/elfgetzdata mem testfile-zgnu32 <<\EOF
1: .text, NOT compressed
2: .zdebug_aranges, GNU compressed, size: 40
3: .zdebug_info, GNU compressed, size: 9a
4: .debug_abbrev, NOT compressed
5: .zdebug_line, GNU compressed, size: 85
6: .shstrtab, NOT compressed
7: .symtab, NOT compressed
8: .strtab, NOT compressed
EOF

testfiles testfile-zgnu32be
testrun_compare ${abs_top_builddir}/tests/elfgetzdata read testfile-zgnu32be <<\EOF
1: .text, NOT compressed
2: .eh_frame, NOT compressed
3: .zdebug_aranges, GNU compressed, size: 40
4: .zdebug_info, GNU compressed, size: 6e
5: .debug_abbrev, NOT compressed
6: .zdebug_line, GNU compressed, size: 85
7: .shstrtab, NOT compressed
8: .symtab, NOT compressed
9: .strtab, NOT compressed
EOF

testrun_compare ${abs_top_builddir}/tests/elfgetzdata mmap testfile-zgnu32be <<\EOF
1: .text, NOT compressed
2: .eh_frame, NOT compressed
3: .zdebug_aranges, GNU compressed, size: 40
4: .zdebug_info, GNU compressed, size: 6e
5: .debug_abbrev, NOT compressed
6: .zdebug_line, GNU compressed, size: 85
7: .shstrtab, NOT compressed
8: .symtab, NOT compressed
9: .strtab, NOT compressed
EOF

testrun_compare ${abs_top_builddir}/tests/elfgetzdata mem testfile-zgnu32be <<\EOF
1: .text, NOT compressed
2: .eh_frame, NOT compressed
3: .zdebug_aranges, GNU compressed, size: 40
4: .zdebug_info, GNU compressed, size: 6e
5: .debug_abbrev, NOT compressed
6: .zdebug_line, GNU compressed, size: 85
7: .shstrtab, NOT compressed
8: .symtab, NOT compressed
9: .strtab, NOT compressed
EOF

testfiles testfile-zgabi32
testrun_compare ${abs_top_builddir}/tests/elfgetzdata read testfile-zgabi32 <<\EOF
1: .text, NOT compressed
2: .debug_aranges, ELF compressed, size: 40
3: .debug_info, ELF compressed, size: 9a
4: .debug_abbrev, NOT compressed
5: .debug_line, ELF compressed, size: 85
6: .shstrtab, NOT compressed
7: .symtab, NOT compressed
8: .strtab, NOT compressed
EOF

testrun_compare ${abs_top_builddir}/tests/elfgetzdata mmap testfile-zgabi32 <<\EOF
1: .text, NOT compressed
2: .debug_aranges, ELF compressed, size: 40
3: .debug_info, ELF compressed, size: 9a
4: .debug_abbrev, NOT compressed
5: .debug_line, ELF compressed, size: 85
6: .shstrtab, NOT compressed
7: .symtab, NOT compressed
8: .strtab, NOT compressed
EOF

testrun_compare ${abs_top_builddir}/tests/elfgetzdata mem testfile-zgabi32 <<\EOF
1: .text, NOT compressed
2: .debug_aranges, ELF compressed, size: 40
3: .debug_info, ELF compressed, size: 9a
4: .debug_abbrev, NOT compressed
5: .debug_line, ELF compressed, size: 85
6: .shstrtab, NOT compressed
7: .symtab, NOT compressed
8: .strtab, NOT compressed
EOF

testfiles testfile-zgabi32be
testrun_compare ${abs_top_builddir}/tests/elfgetzdata read testfile-zgabi32be <<\EOF
1: .text, NOT compressed
2: .eh_frame, NOT compressed
3: .debug_aranges, ELF compressed, size: 40
4: .debug_info, ELF compressed, size: 6e
5: .debug_abbrev, NOT compressed
6: .debug_line, ELF compressed, size: 85
7: .shstrtab, NOT compressed
8: .symtab, NOT compressed
9: .strtab, NOT compressed
EOF

testrun_compare ${abs_top_builddir}/tests/elfgetzdata mmap testfile-zgabi32be <<\EOF
1: .text, NOT compressed
2: .eh_frame, NOT compressed
3: .debug_aranges, ELF compressed, size: 40
4: .debug_info, ELF compressed, size: 6e
5: .debug_abbrev, NOT compressed
6: .debug_line, ELF compressed, size: 85
7: .shstrtab, NOT compressed
8: .symtab, NOT compressed
9: .strtab, NOT compressed
EOF

testrun_compare ${abs_top_builddir}/tests/elfgetzdata mem testfile-zgabi32be <<\EOF
1: .text, NOT compressed
2: .eh_frame, NOT compressed
3: .debug_aranges, ELF compressed, size: 40
4: .debug_info, ELF compressed, size: 6e
5: .debug_abbrev, NOT compressed
6: .debug_line, ELF compressed, size: 85
7: .shstrtab, NOT compressed
8: .symtab, NOT compressed
9: .strtab, NOT compressed
EOF

exit 0
