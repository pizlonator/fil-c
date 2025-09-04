/* Optimized strlen implementation using basic LoongArch instructions.
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
# define STRLEN __strlen_aligned
#else
# define STRLEN strlen
#endif

LEAF(STRLEN, 6)
    move        a1, a0
    bstrins.d   a0, zero, 2, 0
    lu12i.w     a2, 0x01010
    li.w        t0, -1

    ld.d        t2, a0, 0
    andi        t1, a1, 0x7
    ori         a2, a2, 0x101
    slli.d      t1, t1, 3

    bstrins.d   a2, a2, 63, 32
    sll.d       t1, t0, t1
    slli.d      t3, a2, 7
    nor         a3, zero, t3

    orn         t2, t2, t1
    sub.d       t0, t2, a2
    nor         t1, t2, a3
    and         t0, t0, t1


    bnez        t0, L(count_pos)
    addi.d      a0, a0, 8
L(loop_16_7bit):
    ld.d        t2, a0, 0
    sub.d       t1, t2, a2

    and         t0, t1, t3
    bnez        t0, L(more_check)
    ld.d        t2, a0, 8
    sub.d       t1, t2, a2

    and         t0, t1, t3
    addi.d      a0, a0, 16
    beqz        t0, L(loop_16_7bit)
    addi.d      a0, a0, -8

L(more_check):
    nor         t0, t2, a3
    and         t0, t1, t0
    bnez        t0, L(count_pos)
    addi.d      a0, a0, 8


L(loop_16_8bit):
    ld.d        t2, a0, 0
    sub.d       t1, t2, a2
    nor         t0, t2, a3
    and         t0, t0, t1

    bnez        t0, L(count_pos)
    ld.d        t2, a0, 8
    addi.d      a0, a0, 16
    sub.d       t1, t2, a2

    nor         t0, t2, a3
    and         t0, t0, t1
    beqz        t0, L(loop_16_8bit)
    addi.d      a0, a0, -8

L(count_pos):
    ctz.d       t1, t0
    sub.d       a0, a0, a1
    srli.d      t1, t1, 3
    add.d       a0, a0, t1

    jr          ra
END(STRLEN)

libc_hidden_builtin_def (STRLEN)
