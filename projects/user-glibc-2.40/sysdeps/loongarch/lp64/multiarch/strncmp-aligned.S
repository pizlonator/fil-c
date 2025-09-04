/* Optimized strncmp implementation using basic LoongArch instructions.
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
# define STRNCMP __strncmp_aligned
#else
# define STRNCMP strncmp
#endif

LEAF(STRNCMP, 6)
    beqz        a2, L(ret0)
    lu12i.w     a5, 0x01010
    andi        a3, a0, 0x7
    ori         a5, a5, 0x101

    andi        a4, a1, 0x7
    bstrins.d   a5, a5, 63, 32
    li.d        t7, -1
    li.d        t8, 8

    addi.d      a2, a2, -1
    slli.d      a6, a5, 7
    bne         a3, a4, L(unaligned)
    bstrins.d   a0, zero, 2, 0

    bstrins.d   a1, zero, 2, 0
    ld.d        t0, a0, 0
    ld.d        t1, a1, 0
    slli.d      t2, a3, 3


    sub.d       t5, t8, a3
    srl.d       t3, t7, t2
    srl.d       t0, t0, t2
    srl.d       t1, t1, t2

    orn         t0, t0, t3
    orn         t1, t1, t3
    sub.d       t2, t0, a5
    andn        t3, a6, t0

    and         t2, t2, t3
    bne         t0, t1, L(al_end)
    sltu        t4, a2, t5
    sub.d       a2, a2, t5

L(al_loop):
    or          t4, t2, t4
    bnez        t4, L(ret0)
    ldx.d       t0, a0, t8
    ldx.d       t1, a1, t8


    addi.d      t8, t8, 8
    sltui       t4, a2, 8
    addi.d      a2, a2, -8
    sub.d       t2, t0, a5

    andn        t3, a6, t0
    and         t2, t2, t3
    beq         t0, t1, L(al_loop)
    addi.d      a2, a2, 8

L(al_end):
    xor         t3, t0, t1
    or          t2, t2, t3
    ctz.d       t2, t2
    srli.d      t4, t2, 3

    bstrins.d   t2, zero, 2, 0
    srl.d       t0, t0, t2
    srl.d       t1, t1, t2
    andi        t0, t0, 0xff


    andi        t1, t1, 0xff
    sltu        t2, a2, t4
    sub.d       a0, t0, t1
    masknez     a0, a0, t2

    jr          ra
L(ret0):
    move        a0, zero
    jr          ra
    nop

L(unaligned):
    slt         a7, a4, a3
    xor         t0, a0, a1
    maskeqz     t0, t0, a7
    xor         a0, a0, t0

    xor         a1, a1, t0
    andi        a3, a0, 0x7
    andi        a4, a1, 0x7
    bstrins.d   a0, zero, 2, 0


    bstrins.d   a1, zero, 2, 0
    ld.d        t4, a0, 0
    ld.d        t1, a1, 0
    slli.d      t2, a3, 3

    slli.d      t3, a4, 3
    srl.d       t5, t7, t3
    srl.d       t0, t4, t2
    srl.d       t1, t1, t3

    orn         t0, t0, t5
    orn         t1, t1, t5
    bne         t0, t1, L(not_equal)
    sub.d       t6, t8, a4

    sub.d       a4, t2, t3
    sll.d       t2, t7, t2
    sub.d       t5, t8, a3
    orn         t4, t4, t2


    sub.d       t2, t4, a5
    andn        t3, a6, t4
    sltu        t7, a2, t5
    and         t2, t2, t3

    sub.d       a3, zero, a4
    or          t2, t2, t7
    bnez        t2, L(un_end)
    sub.d       t7, t5, t6

    sub.d       a2, a2, t5
    sub.d       t6, t8, t7
L(un_loop):
    srl.d       t5, t4, a4
    ldx.d       t4, a0, t8

    ldx.d       t1, a1, t8
    addi.d      t8, t8, 8
    sll.d       t0, t4, a3
    or          t0, t0, t5


    bne         t0, t1, L(loop_not_equal)
    sub.d       t2, t4, a5
    andn        t3, a6, t4
    sltui       t5, a2, 8

    and         t2, t2, t3
    addi.d      a2, a2, -8
    or          t3, t2, t5
    beqz        t3, L(un_loop)

    addi.d      a2, a2, 8
L(un_end):
    sub.d       t2, t0, a5
    andn        t3, a6, t0
    sltu        t5, a2, t6

    and         t2, t2, t3
    or          t2, t2, t5
    bnez        t2, L(ret0)
    ldx.d       t1, a1, t8


    srl.d       t0, t4, a4
    sub.d       a2, a2, t6
L(not_equal):
    sub.d       t2, t0, a5
    andn        t3, a6, t0

    xor         t4, t0, t1
    and         t2, t2, t3
    or          t2, t2, t4
    ctz.d       t2, t2

    bstrins.d   t2, zero, 2, 0
    srli.d      t4, t2, 3
    srl.d       t0, t0, t2
    srl.d       t1, t1, t2

    andi        t0, t0, 0xff
    andi        t1, t1, 0xff
    sub.d       t2, t0, t1
    sub.d       t3, t1, t0


    masknez     t0, t2, a7
    maskeqz     t1, t3, a7
    sltu        t2, a2, t4
    or          a0, t0, t1

    masknez     a0, a0, t2
    jr          ra
L(loop_not_equal):
    add.d       a2, a2, t7
    b           L(not_equal)
END(STRNCMP)

libc_hidden_builtin_def (STRNCMP)
