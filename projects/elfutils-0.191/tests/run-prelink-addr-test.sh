#! /bin/sh
# Copyright (C) 2011-2013 Red Hat, Inc.
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


# testfile52.c:
#   #include <stdlib.h>
#   int foo() { exit(0); }
#
# gcc -m32 -g -shared testfile52-32.c -o testfile52-32.so
# eu-strip -f testfile52-32.so.debug testfile52-32.so
# cp testfile52-32.so testfile52-32.prelink.so
# prelink -N testfile52-32.prelink.so
# cp testfile52-32.so testfile52-32.noshdrs.so
# prelink -r 0x42000000 testfile52-32.noshdrs.so
# eu-strip --remove-comment --strip-sections testfile52-32.noshdrs.so

testfiles testfile52-32.so testfile52-32.so.debug
testfiles testfile52-32.prelink.so testfile52-32.noshdrs.so
tempfiles testmaps52-32 testfile52-32.noshdrs.so.debug
ln -snf testfile52-32.so.debug testfile52-32.noshdrs.so.debug

cat > testmaps52-32 <<EOF
00111000-00112000 r-xp 00000000 fd:01 1 `pwd`/testfile52-32.so
00112000-00113000 rw-p 00000000 fd:01 1 `pwd`/testfile52-32.so
41000000-41001000 r-xp 00000000 fd:01 2 `pwd`/testfile52-32.prelink.so
41001000-41002000 rw-p 00000000 fd:01 2 `pwd`/testfile52-32.prelink.so
42000000-42001000 r-xp 00000000 fd:01 3 `pwd`/testfile52-32.noshdrs.so
42001000-42002000 rw-p 00000000 fd:01 3 `pwd`/testfile52-32.noshdrs.so
EOF

# Prior to commit 1743d7f, libdwfl would fail on the second address,
# because it didn't notice that prelink added a 0x20-byte offset from
# what the .debug file reports.
testrun_compare ${abs_top_builddir}/src/addr2line -S -M testmaps52-32 \
    0x11140c 0x4100042d 0x4200040e <<\EOF
foo
/home/jistone/src/elfutils/tests/testfile52-32.c:2
foo+0x1
/home/jistone/src/elfutils/tests/testfile52-32.c:2
foo+0x2
/home/jistone/src/elfutils/tests/testfile52-32.c:2
EOF

# Repeat testfile52 for -m64.  The particular REL>RELA issue doesn't exist, but
# we'll make sure the rest works anyway.
testfiles testfile52-64.so testfile52-64.so.debug
testfiles testfile52-64.prelink.so testfile52-64.noshdrs.so
tempfiles testmaps52-64 testfile52-64.noshdrs.so.debug
ln -snf testfile52-64.so.debug testfile52-64.noshdrs.so.debug

cat > testmaps52-64 <<EOF
1000000000-1000001000 r-xp 00000000 fd:11 1 `pwd`/testfile52-64.so
1000001000-1000200000 ---p 00001000 fd:11 1 `pwd`/testfile52-64.so
1000200000-1000201000 rw-p 00000000 fd:11 1 `pwd`/testfile52-64.so
3000000000-3000001000 r-xp 00000000 fd:11 2 `pwd`/testfile52-64.prelink.so
3000001000-3000200000 ---p 00001000 fd:11 2 `pwd`/testfile52-64.prelink.so
3000200000-3000201000 rw-p 00000000 fd:11 2 `pwd`/testfile52-64.prelink.so
3800000000-3800001000 r-xp 00000000 fd:11 3 `pwd`/testfile52-64.noshdrs.so
3800001000-3800200000 ---p 00001000 fd:11 3 `pwd`/testfile52-64.noshdrs.so
3800200000-3800201000 rw-p 00000000 fd:11 3 `pwd`/testfile52-64.noshdrs.so
EOF

testrun_compare ${abs_top_builddir}/src/addr2line -S -M testmaps52-64 \
    0x100000056c 0x300000056d 0x380000056e <<\EOF
foo
/home/jistone/src/elfutils/tests/testfile52-64.c:2
foo+0x1
/home/jistone/src/elfutils/tests/testfile52-64.c:2
foo+0x2
/home/jistone/src/elfutils/tests/testfile52-64.c:2
EOF


# testfile53.c:
#   char foo[0x1000];
#   int main() { return 0; }
#
# gcc -m32 -g testfile53-32.c -o testfile53-32
# eu-strip -f testfile53-32.debug testfile53-32
# cp testfile53-32 testfile53-32.prelink
# prelink -N testfile53-32.prelink
testfiles testfile53-32 testfile53-32.debug testfile53-32.prelink

testrun_compare ${abs_top_builddir}/src/addr2line -S -e testfile53-32 0x8048394 0x8048395 <<\EOF
main
/home/jistone/src/elfutils/tests/testfile53-32.c:2
main+0x1
/home/jistone/src/elfutils/tests/testfile53-32.c:2
EOF

# prelink shuffled some of the sections, but .text is in the same place.
testrun_compare ${abs_top_builddir}/src/addr2line -S -e testfile53-32.prelink 0x8048396 0x8048397 <<\EOF
main+0x2
/home/jistone/src/elfutils/tests/testfile53-32.c:2
main+0x3
/home/jistone/src/elfutils/tests/testfile53-32.c:2
EOF

# Repeat testfile53 in 64-bit, except use foo[0x800] to achieve the same
# prelink section shuffling.
testfiles testfile53-64 testfile53-64.debug testfile53-64.prelink

testrun_compare ${abs_top_builddir}/src/addr2line -S -e testfile53-64 0x400474 0x400475 <<\EOF
main
/home/jistone/src/elfutils/tests/testfile53-64.c:2
main+0x1
/home/jistone/src/elfutils/tests/testfile53-64.c:2
EOF

testrun_compare ${abs_top_builddir}/src/addr2line -S -e testfile53-64.prelink 0x400476 0x400477 <<\EOF
main+0x2
/home/jistone/src/elfutils/tests/testfile53-64.c:2
main+0x3
/home/jistone/src/elfutils/tests/testfile53-64.c:2
EOF


# testfile54.c:
#   extern void * stdin;
#   static void * pstdin = &stdin;
#   void * const foo = &pstdin;
#
# gcc -m32 -g -shared -nostartfiles testfile54-32.c -o testfile54-32.so
# eu-strip -f testfile54-32.so.debug testfile54-32.so
# cp testfile54-32.so testfile54-32.prelink.so
# prelink -N testfile54-32.prelink.so
# cp testfile54-32.so testfile54-32.noshdrs.so
# prelink -r 0x42000000 testfile54-32.noshdrs.so
# eu-strip --remove-comment --strip-sections testfile54-32.noshdrs.so
testfiles testfile54-32.so testfile54-32.so.debug
testfiles testfile54-32.prelink.so testfile54-32.noshdrs.so
tempfiles testmaps54-32

# Note we have no testfile54-32.noshdrs.so.debug link here, so
# this is testing finding the symbols in .dynsym via PT_DYNAMIC.

cat > testmaps54-32 <<EOF
00111000-00112000 r--p 00000000 fd:01 1 `pwd`/testfile54-32.so
00112000-00113000 rw-p 00000000 fd:01 1 `pwd`/testfile54-32.so
41000000-41001000 r--p 00000000 fd:01 2 `pwd`/testfile54-32.prelink.so
41001000-41002000 rw-p 00000000 fd:01 2 `pwd`/testfile54-32.prelink.so
42000000-42001000 r--p 00000000 fd:01 3 `pwd`/testfile54-32.noshdrs.so
42001000-42002000 rw-p 00000000 fd:01 3 `pwd`/testfile54-32.noshdrs.so
EOF

testrun_compare ${abs_top_builddir}/src/addr2line -S -M testmaps54-32 \
    0x1111fc 0x1122a4 0x410001fd 0x410012a5 0x420001fe <<\EOF
foo
??:0
pstdin
??:0
foo+0x1
??:0
pstdin+0x1
??:0
foo+0x2
??:0
EOF

# Repeat testfile64 in 64-bit
testfiles testfile54-64.so testfile54-64.so.debug
testfiles testfile54-64.prelink.so testfile54-64.noshdrs.so
tempfiles testmaps54-64

# Note we have no testfile54-64.noshdrs.so.debug link here, so
# this is testing finding the symbols in .dynsym via PT_DYNAMIC.

cat > testmaps54-64 <<EOF
1000000000-1000001000 r--p 00000000 fd:11 1 `pwd`/testfile54-64.so
1000001000-1000200000 ---p 00001000 fd:11 1 `pwd`/testfile54-64.so
1000200000-1000201000 rw-p 00000000 fd:11 1 `pwd`/testfile54-64.so
3000000000-3000001000 r--p 00000000 fd:11 2 `pwd`/testfile54-64.prelink.so
3000001000-3000200000 ---p 00001000 fd:11 2 `pwd`/testfile54-64.prelink.so
3000200000-3000201000 rw-p 00000000 fd:11 2 `pwd`/testfile54-64.prelink.so
3800000000-3800001000 r--p 00000000 fd:11 3 `pwd`/testfile54-64.noshdrs.so
3800001000-3800200000 ---p 00001000 fd:11 3 `pwd`/testfile54-64.noshdrs.so
3800200000-3800201000 rw-p 00000000 fd:11 3 `pwd`/testfile54-64.noshdrs.so
EOF

testrun_compare ${abs_top_builddir}/src/addr2line -S -M testmaps54-64 \
    0x10000002f8 0x1000200448 0x30000002f9 0x3000200449 0x38000002fa <<\EOF
foo
??:0
pstdin
??:0
foo+0x1
??:0
pstdin+0x1
??:0
foo+0x2
??:0
EOF


# testfile55.c:
#   extern void *stdin;
#   int main() { return !stdin; }
#
# gcc -m32 -g testfile55-32.c -o testfile55-32
# eu-strip -f testfile55-32.debug testfile55-32
# cp testfile55-32 testfile55-32.prelink
# prelink -N testfile55-32.prelink
testfiles testfile55-32 testfile55-32.debug testfile55-32.prelink

testrun_compare ${abs_top_builddir}/src/addr2line -S -e testfile55-32 0x80483b4 0x80483b5 <<\EOF
main
/home/jistone/src/elfutils/tests/testfile55-32.c:2
main+0x1
/home/jistone/src/elfutils/tests/testfile55-32.c:2
EOF

# prelink splits .bss into .dynbss+.bss, so the start of .bss changes, but the
# total size remains the same, and .text doesn't move at all.
testrun_compare ${abs_top_builddir}/src/addr2line -S -e testfile55-32.prelink 0x80483b6 0x80483b7 <<\EOF
main+0x2
/home/jistone/src/elfutils/tests/testfile55-32.c:2
main+0x3
/home/jistone/src/elfutils/tests/testfile55-32.c:2
EOF

# Repeat testfile55 in 64-bit
testfiles testfile55-64 testfile55-64.debug testfile55-64.prelink

testrun_compare ${abs_top_builddir}/src/addr2line -S -e testfile55-64 0x4004b4 0x4004b5 <<\EOF
main
/home/jistone/src/elfutils/tests/testfile55-64.c:2
main+0x1
/home/jistone/src/elfutils/tests/testfile55-64.c:2
EOF

testrun_compare ${abs_top_builddir}/src/addr2line -S -e testfile55-64.prelink 0x4004b6 0x4004b7 <<\EOF
main+0x2
/home/jistone/src/elfutils/tests/testfile55-64.c:2
main+0x3
/home/jistone/src/elfutils/tests/testfile55-64.c:2
EOF
