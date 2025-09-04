/* Optimized memcmp implementation using basic LoongArch instructions.
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
# define MEMCMP_NAME __memcmp_aligned
#else
# define MEMCMP_NAME memcmp
#endif

LEAF(MEMCMP_NAME, 6)
    beqz        a2, L(ret)
    andi        a4, a1, 0x7
    andi        a3, a0, 0x7
    sltu        a5, a4, a3

    xor         t0, a0, a1
    li.w        t8, 8
    maskeqz     t0, t0, a5
    li.w        t7, -1

    xor         a0, a0, t0
    xor         a1, a1, t0
    andi        a3, a0, 0x7
    andi        a4, a1, 0x7

    xor         a0, a0, a3
    xor         a1, a1, a4
    ld.d        t2, a0, 0
    ld.d        t1, a1, 0

    slli.d      t3, a3, 3
    slli.d      t4, a4, 3
    sub.d       a6, t3, t4
    srl.d       t1, t1, t4

    srl.d       t0, t2, t3
    srl.d       t5, t7, t4
    sub.d       t6, t0, t1
    and         t6, t6, t5

    sub.d       t5, t8, a4
    bnez        t6, L(first_out)
    bgeu        t5, a2, L(ret)
    sub.d       a2, a2, t5

    bnez        a6, L(unaligned)
    blt         a2, t8, L(al_less_8bytes)
    andi        t1, a2, 31
    beq         t1, a2, L(al_less_32bytes)

    sub.d       t2, a2, t1
    add.d       a4, a0, t2
    move        a2, t1

L(al_loop):
    ld.d        t0, a0, 8

    ld.d        t1, a1, 8
    ld.d        t2, a0, 16
    ld.d        t3, a1, 16
    ld.d        t4, a0, 24

    ld.d        t5, a1, 24
    ld.d        t6, a0, 32
    ld.d        t7, a1, 32
    addi.d      a0, a0, 32

    addi.d      a1, a1, 32
    bne         t0, t1, L(out1)
    bne         t2, t3, L(out2)
    bne         t4, t5, L(out3)

    bne         t6, t7, L(out4)
    bne         a0, a4, L(al_loop)

L(al_less_32bytes):
    srai.d      a4, a2, 4
    beqz        a4, L(al_less_16bytes)

    ld.d        t0, a0, 8
    ld.d        t1, a1, 8
    ld.d        t2, a0, 16
    ld.d        t3, a1, 16

    addi.d      a0, a0, 16
    addi.d      a1, a1, 16
    addi.d      a2, a2, -16
    bne         t0, t1, L(out1)

    bne         t2, t3, L(out2)

L(al_less_16bytes):
    srai.d      a4, a2, 3
    beqz        a4, L(al_less_8bytes)
    ld.d        t0, a0, 8

    ld.d        t1, a1, 8
    addi.d      a0, a0, 8
    addi.d      a1, a1, 8
    addi.d      a2, a2, -8

    bne         t0, t1, L(out1)

L(al_less_8bytes):
    beqz        a2, L(ret)
    ld.d        t0, a0, 8
    ld.d        t1, a1, 8

    li.d        t7, -1
    slli.d      t2, a2, 3
    sll.d       t2, t7, t2
    sub.d       t3, t0, t1

    andn        t6, t3, t2
    bnez        t6, L(count_diff)

L(ret):
    move        a0, zero
    jr          ra

L(out4):
    move        t0, t6
    move        t1, t7
    sub.d       t6, t6, t7
    b           L(count_diff)

L(out3):
    move        t0, t4
    move        t1, t5
    sub.d       t6, t4, t5
    b           L(count_diff)

L(out2):
    move        t0, t2
    move        t1, t3
L(out1):
    sub.d       t6, t0, t1
    b           L(count_diff)

L(first_out):
    slli.d      t4, a2, 3
    slt         t3, a2, t5
    sll.d       t4, t7, t4
    maskeqz     t4, t4, t3

    andn        t6, t6, t4

L(count_diff):
    ctz.d       t2, t6
    bstrins.d   t2, zero, 2, 0
    srl.d       t0, t0, t2

    srl.d       t1, t1, t2
    andi        t0, t0, 0xff
    andi        t1, t1, 0xff
    sub.d       t2, t0, t1

    sub.d       t3, t1, t0
    masknez     t2, t2, a5
    maskeqz     t3, t3, a5
    or          a0, t2, t3

    jr          ra

L(unaligned):
    sub.d       a7, zero, a6
    srl.d       t0, t2, a6
    blt         a2, t8, L(un_less_8bytes)

    andi        t1, a2, 31
    beq         t1, a2, L(un_less_32bytes)
    sub.d       t2, a2, t1
    add.d       a4, a0, t2

    move        a2, t1

L(un_loop):
    ld.d        t2, a0, 8
    ld.d        t1, a1, 8
    ld.d        t4, a0, 16

    ld.d        t3, a1, 16
    ld.d        t6, a0, 24
    ld.d        t5, a1, 24
    ld.d        t8, a0, 32

    ld.d        t7, a1, 32
    addi.d      a0, a0, 32
    addi.d      a1, a1, 32
    sll.d       a3, t2, a7

    or          t0, a3, t0
    bne         t0, t1, L(out1)
    srl.d       t0, t2, a6
    sll.d       a3, t4, a7

    or          t2, a3, t0
    bne         t2, t3, L(out2)
    srl.d       t0, t4, a6
    sll.d       a3, t6, a7

    or          t4, a3, t0
    bne         t4, t5, L(out3)
    srl.d       t0, t6, a6
    sll.d       a3, t8, a7

    or          t6, t0, a3
    bne         t6, t7, L(out4)
    srl.d       t0, t8, a6
    bne         a0, a4, L(un_loop)

L(un_less_32bytes):
    srai.d      a4, a2, 4
    beqz        a4, L(un_less_16bytes)
    ld.d        t2, a0, 8
    ld.d        t1, a1, 8

    ld.d        t4, a0, 16
    ld.d        t3, a1, 16
    addi.d      a0, a0, 16
    addi.d      a1, a1, 16

    addi.d      a2, a2, -16
    sll.d       a3, t2, a7
    or          t0, a3, t0
    bne         t0, t1, L(out1)

    srl.d       t0, t2, a6
    sll.d       a3, t4, a7
    or          t2, a3, t0
    bne         t2, t3, L(out2)

    srl.d       t0, t4, a6

L(un_less_16bytes):
    srai.d      a4, a2, 3
    beqz        a4, L(un_less_8bytes)
    ld.d        t2, a0, 8

    ld.d        t1, a1, 8
    addi.d      a0, a0, 8
    addi.d      a1, a1, 8
    addi.d      a2, a2, -8

    sll.d       a3, t2, a7
    or          t0, a3, t0
    bne         t0, t1, L(out1)
    srl.d       t0, t2, a6

L(un_less_8bytes):
    beqz        a2, L(ret)
    andi        a7, a7, 63
    slli.d      a4, a2, 3
    bgeu        a7, a4, L(last_cmp)

    ld.d        t2, a0, 8
    sll.d       a3, t2, a7
    or          t0, a3, t0

L(last_cmp):
    ld.d        t1, a1, 8

    li.d        t7, -1
    sll.d       t2, t7, a4
    sub.d       t3, t0, t1
    andn        t6, t3, t2

    bnez        t6, L(count_diff)
    move        a0, zero
    jr          ra
END(MEMCMP_NAME)

libc_hidden_builtin_def (MEMCMP_NAME)
