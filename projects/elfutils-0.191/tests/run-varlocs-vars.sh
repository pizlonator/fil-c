#! /bin/sh
# Copyright (C) 2013, 2021 Red Hat, Inc.
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

# Testfiles generated with:
#
# $ cat foo.c
# int x = 1;
# int y = 2;
#
# for cc in gcc clang; do
#   for v in 4 5; do
#     for w in 32 64; do
#       out="testfile-vars-$cc-dwarf$v-$w.o"
#       "$cc" -m"$w" -Wall -Wextra -gdwarf-"$v" -c foo.c -o "$out"
#     done
#   done
# done

testfiles testfile-vars-clang-dwarf4-32.o
testfiles testfile-vars-clang-dwarf4-64.o
testfiles testfile-vars-clang-dwarf5-32.o
testfiles testfile-vars-clang-dwarf5-64.o
testfiles testfile-vars-gcc-dwarf4-32.o
testfiles testfile-vars-gcc-dwarf4-64.o
testfiles testfile-vars-gcc-dwarf5-32.o
testfiles testfile-vars-gcc-dwarf5-64.o

tempfiles varlocs.out
testrun ${abs_top_builddir}/tests/varlocs --debug --exprlocs -e testfile-vars-clang-dwarf4-32.o | grep exprloc > varlocs.out
diff -u varlocs.out - <<EOF
    location (exprloc) {addr(0x0)}
    location (exprloc) {addr(0x4)}
EOF

testrun ${abs_top_builddir}/tests/varlocs --debug --exprlocs -e testfile-vars-clang-dwarf4-64.o | grep exprloc > varlocs.out
diff -u varlocs.out - <<EOF
    location (exprloc) {addr(0x0)}
    location (exprloc) {addr(0x4)}
EOF

testrun ${abs_top_builddir}/tests/varlocs --debug --exprlocs -e testfile-vars-clang-dwarf5-32.o | grep exprloc > varlocs.out
diff -u varlocs.out - <<EOF
    location (exprloc) {addr: 0x0}
    location (exprloc) {addr: 0x4}
EOF

testrun ${abs_top_builddir}/tests/varlocs --debug --exprlocs -e testfile-vars-clang-dwarf5-32.o | grep exprloc > varlocs.out
diff -u varlocs.out - <<EOF
    location (exprloc) {addr: 0x0}
    location (exprloc) {addr: 0x4}
EOF

testrun ${abs_top_builddir}/tests/varlocs --debug --exprlocs -e testfile-vars-gcc-dwarf4-32.o | grep exprloc > varlocs.out
diff -u varlocs.out - <<EOF
    location (exprloc) {addr(0x0)}
    location (exprloc) {addr(0x4)}
EOF

testrun ${abs_top_builddir}/tests/varlocs --debug --exprlocs -e testfile-vars-gcc-dwarf4-64.o | grep exprloc > varlocs.out
diff -u varlocs.out - <<EOF
    location (exprloc) {addr(0x0)}
    location (exprloc) {addr(0x4)}
EOF

testrun ${abs_top_builddir}/tests/varlocs --debug --exprlocs -e testfile-vars-gcc-dwarf5-32.o | grep exprloc > varlocs.out
diff -u varlocs.out - <<EOF
    location (exprloc) {addr(0x0)}
    location (exprloc) {addr(0x4)}
EOF

testrun ${abs_top_builddir}/tests/varlocs --debug --exprlocs -e testfile-vars-gcc-dwarf5-64.o | grep exprloc > varlocs.out
diff -u varlocs.out - <<EOF
    location (exprloc) {addr(0x0)}
    location (exprloc) {addr(0x4)}
EOF

exit 0
