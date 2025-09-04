/* Optimized unaligned memcpy implementation using basic LoongArch instructions.
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

# define MEMCPY_NAME __memcpy_unaligned

# define LD_64(reg, n)           \
    ld.d        t0, reg, n;      \
    ld.d        t1, reg, n + 8;  \
    ld.d        t2, reg, n + 16; \
    ld.d        t3, reg, n + 24; \
    ld.d        t4, reg, n + 32; \
    ld.d        t5, reg, n + 40; \
    ld.d        t6, reg, n + 48; \
    ld.d        t7, reg, n + 56;

# define ST_64(reg, n)           \
    st.d        t0, reg, n;      \
    st.d        t1, reg, n + 8;  \
    st.d        t2, reg, n + 16; \
    st.d        t3, reg, n + 24; \
    st.d        t4, reg, n + 32; \
    st.d        t5, reg, n + 40; \
    st.d        t6, reg, n + 48; \
    st.d        t7, reg, n + 56;

LEAF(MEMCPY_NAME, 3)
    add.d       a4, a1, a2
    add.d       a3, a0, a2
    li.w        a6, 16
    bge         a6, a2, L(less_16bytes)

    li.w        a6, 128
    blt         a6, a2, L(long_bytes)
    li.w        a6, 64
    blt         a6, a2, L(more_64bytes)

    li.w        a6, 32
    blt         a6, a2, L(more_32bytes)

    ld.d        t0, a1, 0
    ld.d        t1, a1, 8
    ld.d        t2, a4, -16
    ld.d        t3, a4, -8

    st.d        t0, a0, 0
    st.d        t1, a0, 8
    st.d        t2, a3, -16
    st.d        t3, a3, -8
    jr          ra

L(more_64bytes):
    srli.d      t8, a0, 3
    slli.d      t8, t8, 3
    addi.d      t8, t8,  0x8
    sub.d       a7, a0, t8

    ld.d        t0, a1, 0
    sub.d       a1, a1, a7
    st.d        t0, a0, 0
    add.d       a7, a7, a2
    addi.d      a7, a7, -0x20

L(loop_32):
    ld.d        t0, a1, 0
    ld.d        t1, a1, 8
    ld.d        t2, a1, 16
    ld.d        t3, a1, 24

    st.d        t0, t8, 0
    st.d        t1, t8, 8
    st.d        t2, t8, 16
    st.d        t3, t8, 24

    addi.d      t8, t8, 0x20
    addi.d	a1, a1, 0x20
    addi.d	a7, a7, -0x20
    blt         zero, a7, L(loop_32)

    ld.d        t4, a4, -32
    ld.d        t5, a4, -24
    ld.d        t6, a4, -16
    ld.d        t7, a4, -8

    st.d        t4, a3, -32
    st.d        t5, a3, -24
    st.d        t6, a3, -16
    st.d        t7, a3, -8

    jr          ra

L(more_32bytes):
    ld.d        t0, a1, 0
    ld.d        t1, a1, 8
    ld.d        t2, a1, 16
    ld.d        t3, a1, 24

    ld.d        t4, a4, -32
    ld.d        t5, a4, -24
    ld.d        t6, a4, -16
    ld.d        t7, a4, -8

    st.d        t0, a0, 0
    st.d        t1, a0, 8
    st.d        t2, a0, 16
    st.d        t3, a0, 24

    st.d        t4, a3, -32
    st.d        t5, a3, -24
    st.d        t6, a3, -16
    st.d        t7, a3, -8

    jr          ra

L(less_16bytes):
    srai.d      a6, a2, 3
    beqz        a6, L(less_8bytes)

    ld.d        t0, a1, 0
    ld.d        t1, a4, -8
    st.d        t0, a0, 0
    st.d        t1, a3, -8

    jr          ra

L(less_8bytes):
    srai.d      a6, a2, 2
    beqz        a6, L(less_4bytes)

    ld.w        t0, a1, 0
    ld.w        t1, a4, -4
    st.w        t0, a0, 0
    st.w        t1, a3, -4

    jr          ra

L(less_4bytes):
    srai.d      a6, a2, 1
    beqz        a6, L(less_2bytes)

    ld.h        t0, a1, 0
    ld.h        t1, a4, -2
    st.h        t0, a0, 0
    st.h        t1, a3, -2

    jr          ra

L(less_2bytes):
    beqz        a2, L(less_1bytes)

    ld.b        t0, a1, 0
    st.b        t0, a0, 0
    jr          ra

L(less_1bytes):
    jr          ra

L(long_bytes):
    srli.d      t8, a0, 3
    slli.d      t8, t8, 3
    beq         a0, t8, L(start)
    ld.d        t0, a1, 0

    addi.d      t8, t8, 0x8
    st.d        t0, a0, 0
    sub.d       a7, a0, t8
    sub.d       a1, a1, a7

L(start):
    addi.d     a5, a3, -0x80
    blt        a5, t8, L(align_end_proc)

L(loop_128):
    LD_64(a1, 0)
    ST_64(t8, 0)
    LD_64(a1, 64)
    addi.d     a1, a1, 0x80
    ST_64(t8, 64)
    addi.d     t8, t8, 0x80
    bge        a5, t8, L(loop_128)

L(align_end_proc):
    sub.d      a2, a3, t8
    pcaddi     t1, 34
    andi       t2, a2, 0x78
    sub.d      t1, t1, t2
    jr         t1

    ld.d       t0, a1, 112
    st.d       t0, t8, 112
    ld.d       t0, a1, 104
    st.d       t0, t8, 104
    ld.d       t0, a1, 96
    st.d       t0, t8, 96
    ld.d       t0, a1, 88
    st.d       t0, t8, 88
    ld.d       t0, a1, 80
    st.d       t0, t8, 80
    ld.d       t0, a1, 72
    st.d       t0, t8, 72
    ld.d       t0, a1, 64
    st.d       t0, t8, 64
    ld.d       t0, a1, 56
    st.d       t0, t8, 56
    ld.d       t0, a1, 48
    st.d       t0, t8, 48
    ld.d       t0, a1, 40
    st.d       t0, t8, 40
    ld.d       t0, a1, 32
    st.d       t0, t8, 32
    ld.d       t0, a1, 24
    st.d       t0, t8, 24
    ld.d       t0, a1, 16
    st.d       t0, t8, 16
    ld.d       t0, a1, 8
    st.d       t0, t8, 8
    ld.d       t0, a1, 0
    st.d       t0, t8, 0
    ld.d       t0, a4, -8
    st.d       t0, a3, -8

    jr         ra
END(MEMCPY_NAME)

libc_hidden_builtin_def (MEMCPY_NAME)
#endif
