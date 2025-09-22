#! /bin/sh
# Copyright (C) 2018 Facebook, Inc.
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

# This test file is created with
# $ cat t.c
# struct tt {
#   int a;
#   char b;
# };
# int test(struct tt *t) {
#    return t->a;
# }
# $ clang -O2 -g -emit-llvm -c t.c -o - | llc -march=bpf -filetype=obj -o t.o
# $ mv t.o testfile-bpf-reloc.o

testfiles testfile-bpf-reloc.o testfile-bpf-reloc.expect
testrun_compare ${abs_top_builddir}/src/objdump -r testfile-bpf-reloc.o < testfile-bpf-reloc.expect
