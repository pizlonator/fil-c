/* Optimized strcmp implementation using basic LoongArch instructions.
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
# define STRCMP_NAME __strcmp_aligned
#else
# define STRCMP_NAME strcmp
#endif

LEAF(STRCMP_NAME, 6)
    lu12i.w     a4, 0x01010
    andi        a2, a0, 0x7
    ori         a4, a4, 0x101
    andi        a3, a1, 0x7

    bstrins.d   a4, a4, 63, 32
    li.d        t7, -1
    li.d        t8, 8
    slli.d      a5, a4, 7

    bne         a2, a3, L(unaligned)
    bstrins.d   a0, zero, 2, 0
    bstrins.d   a1, zero, 2, 0
    ld.d        t0, a0, 0

    ld.d        t1, a1, 0
    slli.d      t3, a2, 3
    sll.d       t2, t7, t3
    orn         t0, t0, t2


    orn         t1, t1, t2
    sub.d       t2, t0, a4
    andn        t3, a5, t0
    and         t2, t2, t3

    bne         t0, t1, L(al_end)
L(al_loop):
    bnez        t2, L(ret0)
    ldx.d       t0, a0, t8
    ldx.d       t1, a1, t8

    addi.d      t8, t8, 8
    sub.d       t2, t0, a4
    andn        t3, a5, t0
    and         t2, t2, t3

    beq         t0, t1, L(al_loop)
L(al_end):
    xor         t3, t0, t1
    or          t2, t2, t3
    ctz.d       t3, t2


    bstrins.d   t3, zero, 2, 0
    srl.d       t0, t0, t3
    srl.d       t1, t1, t3
    andi        t0, t0, 0xff

    andi        t1, t1, 0xff
    sub.d       a0, t0, t1
    jr          ra
    nop

L(ret0):
    move        a0, zero
    jr          ra
    nop
    nop

L(unaligned):
    slt         a6, a3, a2
    xor         t0, a0, a1
    maskeqz     t0, t0, a6
    xor         a0, a0, t0


    xor         a1, a1, t0
    andi        a2, a0, 0x7
    andi        a3, a1, 0x7
    bstrins.d   a0, zero, 2, 0

    bstrins.d   a1, zero, 2, 0
    ld.d        t4, a0, 0
    ld.d        t1, a1, 0
    slli.d      a2, a2, 3

    slli.d      a3, a3, 3
    srl.d       t0, t4, a2
    srl.d       t1, t1, a3
    srl.d       t5, t7, a3

    orn         t0, t0, t5
    orn         t1, t1, t5
    bne         t0, t1, L(not_equal)
    sll.d       t5, t7, a2


    sub.d       a3, a2, a3
    orn         t4, t4, t5
    sub.d       a2, zero, a3
    sub.d       t2, t4, a4

    andn        t3, a5, t4
    and         t2, t2, t3
    bnez        t2, L(find_zero)
L(un_loop):
    srl.d       t5, t4, a3

    ldx.d       t4, a0, t8
    ldx.d       t1, a1, t8
    addi.d      t8, t8, 8
    sll.d       t0, t4, a2

    or          t0, t0, t5
    bne         t0, t1, L(not_equal)
    sub.d       t2, t4, a4
    andn        t3, a5, t4


    and         t2, t2, t3
    beqz        t2, L(un_loop)
L(find_zero):
    sub.d       t2, t0, a4
    andn        t3, a5, t0

    and         t2, t2, t3
    bnez        t2, L(ret0)
    ldx.d       t1, a1, t8
    srl.d       t0, t4, a3

L(not_equal):
    sub.d       t2, t0, a4
    andn        t3, a5, t0
    and         t2, t2, t3
    xor         t3, t0, t1

    or          t2, t2, t3
L(un_end):
    ctz.d       t3, t2
    bstrins.d   t3, zero, 2, 0
    srl.d       t0, t0, t3


    srl.d       t1, t1, t3
    andi        t0, t0, 0xff
    andi        t1, t1, 0xff
    sub.d       t2, t0, t1


    sub.d       t3, t1, t0
    masknez     t0, t2, a6
    maskeqz     t1, t3, a6
    or          a0, t0, t1

    jr	ra
END(STRCMP_NAME)

libc_hidden_builtin_def (STRCMP_NAME)
