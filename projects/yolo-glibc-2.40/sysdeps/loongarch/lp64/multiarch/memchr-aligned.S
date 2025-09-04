/* Optimized memchr implementation using basic LoongArch instructions.
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
# define MEMCHR_NAME __memchr_aligned
#else
# define MEMCHR_NAME memchr
#endif

LEAF(MEMCHR_NAME, 6)
    beqz        a2, L(out)
    andi        t1, a0, 0x7
    add.d       a5, a0, a2
    bstrins.d   a0, zero, 2, 0

    ld.d        t0, a0, 0
    bstrins.d   a1, a1, 15, 8
    lu12i.w     a3, 0x01010
    slli.d      t2, t1, 03

    bstrins.d   a1, a1, 31, 16
    ori         a3, a3, 0x101
    li.d        t7, -1
    li.d        t8, 8

    bstrins.d   a1, a1, 63, 32
    bstrins.d   a3, a3, 63, 32
    sll.d       t2, t7, t2
    xor         t0, t0, a1


    addi.d      a6, a5, -1
    slli.d      a4, a3, 7
    sub.d       t1, t8, t1
    orn         t0, t0, t2

    sub.d       t2, t0, a3
    andn        t3, a4, t0
    bstrins.d   a6, zero, 2, 0
    and         t0, t2, t3

    bgeu        t1, a2, L(end)
L(loop):
    bnez        t0, L(found)
    ld.d        t1, a0, 8
    xor         t0, t1, a1

    addi.d      a0, a0, 8
    sub.d       t2, t0, a3
    andn        t3, a4, t0
    and         t0, t2, t3


    bne         a0, a6, L(loop)
L(end):
    sub.d       t1, a5, a6
    ctz.d       t0, t0
    srli.d      t0, t0, 3

    sltu        t1, t0, t1
    add.d       a0, a0, t0
    maskeqz     a0, a0, t1
    jr          ra

L(found):
    ctz.d       t0, t0
    srli.d      t0, t0, 3
    add.d       a0, a0, t0
    jr          ra

L(out):
    move        a0, zero
    jr          ra
END(MEMCHR_NAME)

libc_hidden_builtin_def (MEMCHR_NAME)
