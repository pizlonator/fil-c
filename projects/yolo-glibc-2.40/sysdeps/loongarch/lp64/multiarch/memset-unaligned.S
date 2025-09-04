/* Optimized memset unaligned implementation using basic LoongArch instructions.
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

# define MEMSET_NAME __memset_unaligned

#define ST_128(n)              \
    st.d        a1, a0, n;     \
    st.d        a1, a0, n+8  ; \
    st.d        a1, a0, n+16 ; \
    st.d        a1, a0, n+24 ; \
    st.d        a1, a0, n+32 ; \
    st.d        a1, a0, n+40 ; \
    st.d        a1, a0, n+48 ; \
    st.d        a1, a0, n+56 ; \
    st.d        a1, a0, n+64 ; \
    st.d        a1, a0, n+72 ; \
    st.d        a1, a0, n+80 ; \
    st.d        a1, a0, n+88 ; \
    st.d        a1, a0, n+96 ; \
    st.d        a1, a0, n+104; \
    st.d        a1, a0, n+112; \
    st.d        a1, a0, n+120;

LEAF(MEMSET_NAME, 6)
    bstrins.d   a1, a1, 15, 8
    add.d       t7, a0, a2
    bstrins.d   a1, a1, 31, 16
    move        t0, a0

    bstrins.d   a1, a1, 63, 32
    srai.d      t8, a2, 4
    beqz        t8, L(less_16bytes)
    srai.d      t8, a2, 6

    bnez        t8, L(more_64bytes)
    srai.d      t8, a2, 5
    beqz        t8, L(less_32bytes)

    st.d        a1, a0, 0
    st.d        a1, a0, 8
    st.d        a1, a0, 16
    st.d        a1, a0, 24

    st.d        a1, t7, -32
    st.d        a1, t7, -24
    st.d        a1, t7, -16
    st.d        a1, t7, -8

    jr          ra

L(less_32bytes):
    st.d        a1, a0, 0
    st.d        a1, a0, 8
    st.d        a1, t7, -16
    st.d        a1, t7, -8

    jr          ra

L(less_16bytes):
    srai.d      t8, a2, 3
    beqz        t8, L(less_8bytes)
    st.d        a1, a0, 0
    st.d        a1, t7, -8

    jr          ra

L(less_8bytes):
    srai.d      t8, a2, 2
    beqz        t8, L(less_4bytes)
    st.w        a1, a0, 0
    st.w        a1, t7, -4

    jr          ra

L(less_4bytes):
    srai.d      t8, a2, 1
    beqz        t8, L(less_2bytes)
    st.h        a1, a0, 0
    st.h        a1, t7, -2

    jr          ra

L(less_2bytes):
    beqz        a2, L(less_1bytes)
    st.b        a1, a0, 0

    jr          ra

L(less_1bytes):
    jr          ra

L(more_64bytes):
    srli.d      a0, a0, 3
    slli.d      a0, a0, 3
    addi.d      a0, a0, 0x8
    st.d        a1, t0, 0

    sub.d       t2, t0, a0
    add.d       a2, t2, a2
    addi.d      a2, a2, -0x80
    blt         a2, zero, L(end_unalign_proc)

L(loop_less):
    ST_128(0)
    addi.d      a0, a0,  0x80
    addi.d      a2, a2, -0x80
    bge         a2, zero, L(loop_less)

L(end_unalign_proc):
    addi.d      a2, a2, 0x80
    pcaddi      t1, 20
    andi        t5, a2, 0x78
    srli.d      t5, t5, 1

    sub.d       t1, t1, t5
    jr          t1

    st.d        a1, a0, 112
    st.d        a1, a0, 104
    st.d        a1, a0, 96
    st.d        a1, a0, 88
    st.d        a1, a0, 80
    st.d        a1, a0, 72
    st.d        a1, a0, 64
    st.d        a1, a0, 56
    st.d        a1, a0, 48
    st.d        a1, a0, 40
    st.d        a1, a0, 32
    st.d        a1, a0, 24
    st.d        a1, a0, 16
    st.d        a1, a0, 8
    st.d        a1, a0, 0
    st.d        a1, t7, -8

    move        a0, t0
    jr          ra
END(MEMSET_NAME)

libc_hidden_builtin_def (MEMSET_NAME)
#endif
