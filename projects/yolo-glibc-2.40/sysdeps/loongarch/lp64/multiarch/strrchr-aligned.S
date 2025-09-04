/* Optimized strrchr implementation using basic LoongArch instructions.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#include <sysdep.h>
#include <sys/regdef.h>
#include <sys/asm.h>

#if IS_IN (libc)
# define STRRCHR __strrchr_aligned
#else
# define STRRCHR strrchr
#endif

LEAF(STRRCHR, 6)
    slli.d      t0, a0, 3
    bstrins.d   a0, zero, 2, 0
    lu12i.w     a2, 0x01010
    ld.d        t2, a0, 0

    andi        a1, a1, 0xff
    ori         a2, a2, 0x101
    li.d        t3, -1
    bstrins.d	a2, a2, 63, 32

    sll.d       t5, t3, t0
    slli.d      a3, a2, 7
    orn         t4, t2, t5
    mul.d       a1, a1, a2

    sub.d       t0, t4, a2
    andn        t1, a3, t4
    and         t1, t0, t1
    beqz        t1, L(find_tail)


    ctz.d       t0, t1
    orn         t0, zero, t0
    xor         t2, t4, a1
    srl.d       t0, t3, t0

    orn         t2, t2, t0
    orn         t2, t2, t5
    revb.d      t2, t2
    sub.d       t1, t2, a2

    andn        t0, a3, t2
    and         t1, t0, t1
    ctz.d       t0, t1
    srli.d      t0, t0, 3

    addi.d      a0, a0, 7
    sub.d       a0, a0, t0
    maskeqz     a0, a0, t1
    jr          ra


L(find_tail):
    addi.d      a4, a0, 8
    addi.d      a0, a0, 8
L(loop_ascii):
    ld.d        t2, a0, 0
    sub.d       t1, t2, a2

    and         t0, t1, a3
    bnez        t0, L(more_check)
    ld.d        t2, a0, 8
    sub.d       t1, t2, a2

    and         t0, t1, a3
    addi.d      a0, a0, 16
    beqz        t0, L(loop_ascii)
    addi.d      a0, a0, -8

L(more_check):
    andn        t0, a3, t2
    and         t1, t1, t0
    bnez        t1, L(tail)
    addi.d      a0, a0, 8


L(loop_nonascii):
    ld.d        t2, a0, 0
    sub.d       t1, t2, a2
    andn        t0, a3, t2
    and         t1, t0, t1

    bnez        t1, L(tail)
    ld.d        t2, a0, 8
    addi.d      a0, a0, 16
    sub.d       t1, t2, a2

    andn        t0, a3, t2
    and         t1, t0, t1
    beqz        t1, L(loop_nonascii)
    addi.d      a0, a0, -8

L(tail):
    ctz.d       t0, t1
    orn         t0, zero, t0
    xor         t2, t2, a1
    srl.d       t0, t3, t0


    orn         t2, t2, t0
    revb.d      t2, t2
    sub.d       t1, t2, a2
    andn        t0, a3, t2

    and         t1, t0, t1
    bnez        t1, L(count_pos)
L(find_loop):
    beq         a0, a4, L(find_end)
    ld.d        t2, a0, -8

    addi.d      a0, a0, -8
    xor         t2, t2, a1
    sub.d       t1, t2, a2
    andn        t0, a3, t2

    and         t1, t0, t1
    beqz        t1, L(find_loop)
    revb.d      t2, t2
    sub.d       t1, t2, a2


    andn        t0, a3, t2
    and         t1, t0, t1
L(count_pos):
    ctz.d       t0, t1
    addi.d      a0, a0, 7

    srli.d      t0, t0, 3
    sub.d       a0, a0, t0
    jr          ra
    nop

L(find_end):
    xor         t2, t4, a1
    orn         t2, t2, t5
    revb.d      t2, t2
    sub.d       t1, t2, a2


    andn        t0, a3, t2
    and         t1, t0, t1
    ctz.d       t0, t1
    srli.d      t0, t0, 3

    addi.d      a0, a4, -1
    sub.d       a0, a0, t0
    maskeqz     a0, a0, t1
    jr          ra
END(STRRCHR)

libc_hidden_builtin_def(STRRCHR)
