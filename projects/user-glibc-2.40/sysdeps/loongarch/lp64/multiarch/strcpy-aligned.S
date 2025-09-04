/* Optimized strcpy stpcpy aligned implementation using basic LoongArch
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
#  define STRCPY __strcpy_aligned
# endif
#else
# ifndef STRCPY
#  define STRCPY strcpy
# endif
#endif

LEAF(STRCPY, 6)
    andi        a3, a0, 0x7
    move        a2, a0
    beqz        a3, L(dest_align)
    sub.d       a5, a1, a3
    addi.d      a5, a5, 8

L(make_dest_align):
    ld.b        t0, a1, 0
    addi.d      a1, a1, 1
    st.b        t0, a2, 0
    addi.d      a2, a2, 1
    beqz        t0, L(al_out)

    bne         a1, a5, L(make_dest_align)

L(dest_align):
    andi        a4, a1, 7
    bstrins.d   a1, zero, 2, 0

    lu12i.w     t5, 0x1010
    ld.d        t0, a1, 0
    ori         t5, t5, 0x101
    bstrins.d   t5, t5, 63, 32

    slli.d      t6, t5, 0x7
    bnez        a4, L(unalign)
    sub.d       t1, t0, t5
    andn        t2, t6, t0

    and         t3, t1, t2
    bnez        t3, L(al_end)

L(al_loop):
    st.d        t0, a2, 0
    ld.d        t0, a1, 8

    addi.d      a1, a1, 8
    addi.d      a2, a2, 8
    sub.d       t1, t0, t5
    andn        t2, t6, t0

    and         t3, t1, t2
    beqz        t3, L(al_loop)

L(al_end):
    ctz.d       t1, t3
    srli.d      t1, t1, 3
    addi.d      t1, t1, 1

    andi        a3, t1, 8
    andi        a4, t1, 4
    andi        a5, t1, 2
    andi        a6, t1, 1

L(al_end_8):
    beqz        a3, L(al_end_4)
    st.d        t0, a2, 0
#ifdef USE_AS_STPCPY
    addi.d      a0, a2, 7
#endif
    jr          ra
L(al_end_4):
    beqz        a4, L(al_end_2)
    st.w        t0, a2, 0
    addi.d      a2, a2, 4
    srli.d      t0, t0, 32
L(al_end_2):
    beqz        a5, L(al_end_1)
    st.h        t0, a2, 0
    addi.d      a2, a2, 2
    srli.d      t0, t0, 16
L(al_end_1):
    beqz        a6, L(al_out)
    st.b        t0, a2, 0
    addi.d      a2, a2, 1
L(al_out):
#ifdef USE_AS_STPCPY
    addi.d      a0, a2, -1
#endif
    jr          ra

    .align      4
L(unalign):
    slli.d      a5, a4, 3
    li.d        t1, -1
    sub.d       a6, zero, a5

    srl.d       a7, t0, a5
    sll.d       t7, t1, a6

    or          t0, a7, t7
    sub.d       t1, t0, t5
    andn        t2, t6, t0
    and         t3, t1, t2

    bnez        t3, L(un_end)

    ld.d        t4, a1, 8

    sub.d       t1, t4, t5
    andn        t2, t6, t4
    sll.d       t0, t4, a6
    and         t3, t1, t2

    or          t0, t0, a7
    bnez        t3, L(un_end_with_remaining)

L(un_loop):
    srl.d       a7, t4, a5

    ld.d        t4, a1, 16
    addi.d      a1, a1, 8

    st.d        t0, a2, 0
    addi.d      a2, a2, 8

    sub.d       t1, t4, t5
    andn        t2, t6, t4
    sll.d       t0, t4, a6
    and         t3, t1, t2

    or          t0, t0, a7
    beqz        t3, L(un_loop)

L(un_end_with_remaining):
    ctz.d       t1, t3
    srli.d      t1, t1, 3
    addi.d      t1, t1, 1
    sub.d       t1, t1, a4

    blt         t1, zero, L(un_end_less_8)
    st.d        t0, a2, 0
    addi.d      a2, a2, 8
    beqz        t1, L(un_out)
    srl.d       t0, t4, a5
    b           L(un_end_less_8)

L(un_end):
    ctz.d       t1, t3
    srli.d      t1, t1, 3
    addi.d      t1, t1, 1

L(un_end_less_8):
    andi        a4, t1, 4
    andi        a5, t1, 2
    andi        a6, t1, 1
L(un_end_4):
    beqz        a4, L(un_end_2)
    st.w        t0, a2, 0
    addi.d      a2, a2, 4
    srli.d      t0, t0, 32
L(un_end_2):
    beqz        a5, L(un_end_1)
    st.h        t0, a2, 0
    addi.d      a2, a2, 2
    srli.d      t0, t0, 16
L(un_end_1):
    beqz        a6, L(un_out)
    st.b        t0, a2, 0
    addi.d      a2, a2, 1
L(un_out):
#ifdef USE_AS_STPCPY
    addi.d      a0, a2, -1
#endif
    jr          ra
END(STRCPY)

libc_hidden_builtin_def (STRCPY)
