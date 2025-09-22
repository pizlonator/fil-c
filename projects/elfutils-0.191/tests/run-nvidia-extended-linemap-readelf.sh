# Copyright (C) 2011 Red Hat, Inc.
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

# NOTE:
#   the file testfile_nvidia_linemap is a CUDA binary for an NVIDIA A100 generated as follows using CUDA 11.2
#   nvcc -o main main.cu  -Xcompiler "-g -fopenmp" -O3 -lineinfo -arch sm_80  -lcudart -lcuda -lstdc++ -lm
#   cuobjdump -xelf all main
#   mv main.sm_80.cubin testfile_nvidia_linemap

testfiles testfile_nvidia_linemap
testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=line testfile_nvidia_linemap << EOF

DWARF section [ 5] '.debug_line' at offset 0x3e0:

Table at offset 0:

 Length:                         253
 DWARF version:                  2
 Prologue length:                111
 Address size:                   8
 Segment selector size:          0
 Min instruction length:         1
 Max operations per instruction: 1
 Initial value if 'is_stmt':     1
 Line base:                      -5
 Line range:                     14
 Opcode base:                    10

Opcodes:
  [1]  0 arguments
  [2]  1 argument
  [3]  1 argument
  [4]  1 argument
  [5]  1 argument
  [6]  0 arguments
  [7]  0 arguments
  [8]  0 arguments
  [9]  1 argument

Directory table:
 /home/johnmc/hpctoolkit-gpu-samples/nvidia_extended_linemap4

File name table:
 Entry Dir   Time      Size      Name
 1     1     1626104146 1819      main.cu
 2     1     1626104111 211       bar.h

Line number statements:
 [    79] extended opcode 2:  set address to 0 <kernel>
 [    84] set file to 1
 [    86] advance line by constant 24 to 25
 [    88] copy
 [    89] special opcode 240: address+16 = 0x10 <kernel+0x10>, line+1 = 26
 [    8a] advance line by constant 1 to 27
 [    8c] advance address by 48 to 0x40 <kernel+0x40>
 [    8e] copy
 [    8f] advance line by constant -2 to 25
 [    91] advance address by 80 to 0x90 <kernel+0x90>
 [    94] copy
 [    95] special opcode 242: address+16 = 0xa0 <kernel+0xa0>, line+3 = 28
 [    96] advance address by 96 to 0x100 <kernel+0x100>
 [    99] copy
 [    9a] extended opcode 144:  set inlined context 6, function name foo (0x0)
 [    9f] advance line by constant -20 to 8
 [    a1] copy
 [    a2] advance line by constant 1 to 9
 [    a4] advance address by 80 to 0x150 <kernel+0x150>
 [    a7] copy
 [    a8] extended opcode 144:  set inlined context 0, function name foo (0x0)
 [    ad] advance line by constant 22 to 31
 [    af] advance address by 144 to 0x1e0 <kernel+0x1e0>
 [    b2] copy
 [    b3] set file to 2
 [    b5] extended opcode 144:  set inlined context 9, function name bar (0x4)
 [    ba] advance line by constant -25 to 6
 [    bc] copy
 [    bd] set file to 1
 [    bf] extended opcode 144:  set inlined context 10, function name foo (0x0)
 [    c4] advance line by constant 2 to 8
 [    c6] copy
 [    c7] advance line by constant 1 to 9
 [    c9] advance address by 64 to 0x220 <kernel+0x220>
 [    cc] copy
 [    cd] set file to 2
 [    cf] extended opcode 144:  set inlined context 9, function name bar (0x4)
 [    d4] advance line by constant -2 to 7
 [    d6] advance address by 144 to 0x2b0 <kernel+0x2b0>
 [    d9] copy
 [    da] advance line by constant 1 to 8
 [    dc] advance address by 64 to 0x2f0 <kernel+0x2f0>
 [    df] copy
 [    e0] set file to 1
 [    e2] extended opcode 144:  set inlined context 14, function name _Z1aPiS_S_ (0x8)
 [    e7] advance line by constant 10 to 18
 [    e9] copy
 [    ea] advance line by constant 1 to 19
 [    ec] advance address by 64 to 0x330 <kernel+0x330>
 [    ef] copy
 [    f0] extended opcode 144:  set inlined context 0, function name foo (0x0)
 [    f5] advance line by constant 14 to 33
 [    f7] advance address by 144 to 0x3c0 <kernel+0x3c0>
 [    fa] copy
 [    fb] advance address by 192 to 0x480
 [    fe] extended opcode 1:  end of sequence
EOF
