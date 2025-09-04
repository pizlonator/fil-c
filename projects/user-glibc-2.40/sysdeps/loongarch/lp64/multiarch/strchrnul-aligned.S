/* Optimized strchrnul implementation using basic LoongArch instructions.
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
# define STRCHRNUL_NAME __strchrnul_aligned
#else
# define STRCHRNUL_NAME __strchrnul
#endif

LEAF(STRCHRNUL_NAME, 6)
    slli.d      t1, a0, 3
    bstrins.d   a0, zero, 2, 0
    lu12i.w     a2, 0x01010
    ld.d        t2, a0, 0

    ori         a2, a2, 0x101
    andi        a1, a1, 0xff
    bstrins.d   a2, a2, 63, 32
    li.w        t0, -1

    mul.d       a1, a1, a2
    sll.d       t0, t0, t1
    slli.d      a3, a2, 7
    orn         t2, t2, t0

    sll.d       t3, a1, t1
    xor         t4, t2, t3
    sub.d       a4, t2, a2
    sub.d       a5, t4, a2


    andn        a4, a4, t2
    andn        a5, a5, t4
    or          t0, a4, a5
    and         t0, t0, a3

    bnez        t0, L(end)
    addi.d      a0, a0, 8
L(loop):
    ld.d        t4, a0, 0
    xor         t2, t4, a1

    sub.d       a4, t4, a2
    sub.d       a5, t2, a2
    andn        a4, a4, t4
    andn        a5, a5, t2

    or          t0, a4, a5
    and         t0, t0, a3
    bnez        t0, L(end)
    ld.d        t4, a0, 8


    addi.d      a0, a0, 16
    xor         t2, t4, a1
    sub.d       a4, t4, a2
    sub.d       a5, t2, a2

    andn        a4, a4, t4
    andn        a5, a5, t2
    or          t0, a4, a5
    and         t0, t0, a3

    beqz        t0, L(loop)
    addi.d      a0, a0, -8
L(end):
    ctz.d       t0, t0
    srli.w      t0, t0, 3


    add.d       a0, a0, t0
    jr          ra
END(STRCHRNUL_NAME)

libc_hidden_builtin_def (STRCHRNUL_NAME)
