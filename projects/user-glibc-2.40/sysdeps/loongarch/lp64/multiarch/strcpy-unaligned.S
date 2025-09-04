/* Optimized strcpy unaligned implementation using basic LoongArch
   instructions.
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

# ifndef STRCPY
#  define STRCPY __strcpy_unaligned
# endif

# ifdef USE_AS_STPCPY
#  define dstend a0
# else
#  define dstend a4
# endif

LEAF(STRCPY, 6)
    lu12i.w     t5, 0x01010
    li.w        t0, 0xff8
    ori         t5, t5, 0x101
    andi        t1, a1, 0xfff

    bstrins.d   t5, t5, 63, 32
    move        a2, a0
    slli.d      t6, t5, 7
    bltu        t0, t1, L(page_cross)

L(start_entry):
    ld.d        t0, a1, 0
    li.d        t3, 8
    andi        a3, a1, 0x7
    sub.d       t1, t0, t5

    andn        t2, t6, t0
    sub.d       t3, t3, a3
    and         t1, t1, t2
    bnez        t1, L(end)


    add.d       a1, a1, t3
    st.d        t0, a2, 0
    add.d       a2, a2, t3
    ld.d        t0, a1, 0

    sub.d       t1, t0, t5
    andn        t2, t6, t0
    and         t1, t1, t2
    bnez        t1, L(long_end)

L(loop):
    st.d        t0, a2, 0
    ld.d        t0, a1, 8
    addi.d      a2, a2, 8
    addi.d      a1, a1, 8

    sub.d       t1, t0, t5
    andn        t2, t6, t0
    and         t1, t1, t2
    beqz        t1, L(loop)


L(long_end):
    ctz.d       t1, t1
    srli.d      t1, t1, 3
    add.d       a1, a1, t1
    ld.d        t0, a1, -7

    add.d       dstend, a2, t1
    st.d        t0, dstend, -7
    jr          ra
    nop

L(end):
    ctz.d       t1, t1
    srli.d      t1, t1, 3
    add.d       a3, a1, t1
    add.d       dstend, a2, t1

L(less_8):
    li.d        t0, 3
    bltu        t1, t0, L(less_3)
    ld.w        t1, a1, 0
    ld.w        t2, a3, -3


    st.w        t1, a2, 0
    st.w        t2, dstend, -3
    jr          ra
L(less_3):
    beqz        t1, L(zero_bytes)

    ld.h        t1, a1, 0
    st.h        t1, a2, 0
L(zero_bytes):
    st.b        zero, dstend, 0
    jr          ra

L(page_cross):
    move        a4, a1
    bstrins.d   a4, zero, 2, 0
    ld.d        t0, a4, 0
    li.d        t3, -1

    slli.d      t4, a1, 3
    srl.d       t3, t3, t4
    srl.d       t0, t0, t4
    orn         t0, t0, t3


    sub.d       t1, t0, t5
    andn        t2, t6, t0
    and         t1, t1, t2
    beqz        t1, L(start_entry)

    b           L(end)
END(STRCPY)

libc_hidden_builtin_def (STRCPY)
#endif
