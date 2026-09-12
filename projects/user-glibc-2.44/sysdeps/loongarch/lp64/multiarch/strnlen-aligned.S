/* Optimized strnlen implementation using basic LoongArch instructions.
   Copyright (C) 2023-2026 Free Software Foundation, Inc.

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
# define STRNLEN __strnlen_aligned
#else
# define STRNLEN __strnlen
#endif

LEAF(STRNLEN, 6)
    beqz        a1, L(out)
    lu12i.w     a2, 0x01010
    andi        t1, a0, 0x7
    move        t4, a0

    bstrins.d   a0, zero, 2, 0
    ori         a2, a2, 0x101
    li.w        t0, -1
    ld.d        t2, a0, 0

    slli.d      t3, t1, 3
    bstrins.d   a2, a2, 63, 32
    li.w        t5, 8
    slli.d      a3, a2, 7

    sub.w       t1, t5, t1
    sll.d       t0, t0, t3
    orn         t2, t2, t0
    sub.d       t0, t2, a2


    andn        t3, a3, t2
    and         t0, t0, t3
    bnez        t0, L(count_pos)
    sub.d       t5, a1, t1

    bgeu        t1, a1, L(out)
    addi.d      a0, a0, 8
L(loop):
    ld.d        t2, a0, 0
    sub.d       t0, t2, a2

    andn        t1, a3, t2
    sltui       t6, t5, 9
    and         t0, t0, t1
    or          t7, t0, t6

    bnez        t7, L(count_pos)
    ld.d        t2, a0, 8
    addi.d      a0, a0, 16
    sub.d       t0, t2, a2


    andn        t1, a3, t2
    sltui       t6, t5, 17
    and         t0, t0, t1
    addi.d      t5, t5, -16

    or          t7, t0, t6
    beqz        t7, L(loop)
    addi.d      a0, a0, -8
L(count_pos):
    ctz.d       t1, t0

    sub.d       a0, a0, t4
    srli.d      t1, t1, 3
    add.d       a0, t1, a0
    sltu        t0, a0, a1

    masknez     t1, a1, t0
    maskeqz     a0, a0, t0
    or          a0, a0, t1
    jr          ra


L(out):
    move        a0, a1
    jr          ra
END(STRNLEN)

#if !IS_IN (libc)
weak_alias (STRNLEN, strnlen)
libc_hidden_builtin_def (STRNLEN)
#endif
