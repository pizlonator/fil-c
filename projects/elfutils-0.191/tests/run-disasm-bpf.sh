#! /bin/sh
# Copyright (C) 2016 Red Hat, Inc.
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
#
# #include <linux/bpf.h>
# #include <stdio.h>
#
# int main()
# {
#   int i;
#
#   printf("\t.text\n");
#
#   for (i = 0; i < 256; ++i)
#     if (i == (BPF_LD | BPF_IMM | BPF_DW))
#       printf("\t.byte\t%d, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0\n", i);
#     else
#       {
#         int regs = 0;
#         switch (BPF_CLASS(i))
#           {
#           case BPF_ALU:
#           case BPF_ALU64:
#             if (BPF_SRC(i) == BPF_X
#                 && BPF_OP(i) != BPF_NEG
#                 && BPF_OP(i) != BPF_END)
#               regs = 0x21;
#             break;
#           case BPF_LDX:
#           case BPF_STX:
#             regs = 0x21;
#             break;
#           }
#         printf("\t.byte\t%d, %d, 0, 0, 0, 0, 0, 0\n", i, regs);
#       }
#
#   return 0;
# }
#
# $ ./a.out | as -o z1.o
# $ objcopy -j .text z1.o z2.o
#
# Then emacs hexl edit e_machine to 0xf7.

testfiles testfile-bpf-dis1.o testfile-bpf-dis1.expect
testrun_compare ${abs_top_builddir}/src/objdump -d testfile-bpf-dis1.o < testfile-bpf-dis1.expect
