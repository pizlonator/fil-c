/* Optimized memmove_unaligned implementation using basic LoongArch instructions.
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

# define MEMMOVE_NAME __memmove_unaligned

# define LD_64(reg, n)            \
    ld.d        t0, reg, n;      \
    ld.d        t1, reg, n + 8;  \
    ld.d        t2, reg, n + 16; \
    ld.d        t3, reg, n + 24; \
    ld.d        t4, reg, n + 32; \
    ld.d        t5, reg, n + 40; \
    ld.d        t6, reg, n + 48; \
    ld.d        t7, reg, n + 56;

# define ST_64(reg, n)            \
    st.d        t0, reg, n;      \
    st.d        t1, reg, n + 8;  \
    st.d        t2, reg, n + 16; \
    st.d        t3, reg, n + 24; \
    st.d        t4, reg, n + 32; \
    st.d        t5, reg, n + 40; \
    st.d        t6, reg, n + 48; \
    st.d        t7, reg, n + 56;

LEAF(MEMMOVE_NAME, 3)
    add.d       a4, a1, a2
    add.d       a3, a0, a2
    beq         a1, a0, L(less_1bytes)
    move        t8, a0

    srai.d      a6, a2, 4
    beqz        a6, L(less_16bytes)
    srai.d      a6, a2, 6
    bnez        a6, L(more_64bytes)
    srai.d      a6, a2, 5
    beqz        a6, L(less_32bytes)

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

L(less_32bytes):
    ld.d        t0, a1, 0
    ld.d        t1, a1, 8
    ld.d        t2, a4, -16
    ld.d        t3, a4, -8

    st.d        t0, a0, 0
    st.d        t1, a0, 8
    st.d        t2, a3, -16
    st.d        t3, a3, -8

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

L(more_64bytes):
    sub.d       a7, a0, a1
    bltu        a7, a2, L(copy_backward)

L(copy_forward):
    srli.d      a0, a0, 3
    slli.d      a0, a0, 3
    beq         a0, t8, L(all_align)
    addi.d      a0, a0, 0x8
    sub.d       a7, t8, a0
    sub.d       a1, a1, a7
    add.d       a2, a7, a2

L(start_unalign_proc):
    pcaddi      t1, 18
    slli.d      a6, a7, 3
    add.d       t1, t1, a6
    jr          t1

    ld.b        t0, a1, -7
    st.b        t0, a0, -7
    ld.b        t0, a1, -6
    st.b        t0, a0, -6
    ld.b        t0, a1, -5
    st.b        t0, a0, -5
    ld.b        t0, a1, -4
    st.b        t0, a0, -4
    ld.b        t0, a1, -3
    st.b        t0, a0, -3
    ld.b        t0, a1, -2
    st.b        t0, a0, -2
    ld.b        t0, a1, -1
    st.b        t0, a0, -1
L(start_over):

    addi.d      a2, a2, -0x80
    blt         a2, zero, L(end_unalign_proc)

L(loop_less):
    LD_64(a1, 0)
    ST_64(a0, 0)
    LD_64(a1, 64)
    ST_64(a0, 64)

    addi.d      a0, a0, 0x80
    addi.d      a1, a1, 0x80
    addi.d      a2, a2, -0x80
    bge         a2, zero, L(loop_less)

L(end_unalign_proc):
    addi.d      a2, a2, 0x80

    pcaddi      t1, 36
    andi        t2, a2, 0x78
    add.d       a1, a1, t2
    add.d       a0, a0, t2
    sub.d       t1, t1, t2
    jr          t1

    ld.d        t0, a1, -120
    st.d        t0, a0, -120
    ld.d        t0, a1, -112
    st.d        t0, a0, -112
    ld.d        t0, a1, -104
    st.d        t0, a0, -104
    ld.d        t0, a1, -96
    st.d        t0, a0, -96
    ld.d        t0, a1, -88
    st.d        t0, a0, -88
    ld.d        t0, a1, -80
    st.d        t0, a0, -80
    ld.d        t0, a1, -72
    st.d        t0, a0, -72
    ld.d        t0, a1, -64
    st.d        t0, a0, -64
    ld.d        t0, a1, -56
    st.d        t0, a0, -56
    ld.d        t0, a1, -48
    st.d        t0, a0, -48
    ld.d        t0, a1, -40
    st.d        t0, a0, -40
    ld.d        t0, a1, -32
    st.d        t0, a0, -32
    ld.d        t0, a1, -24
    st.d        t0, a0, -24
    ld.d        t0, a1, -16
    st.d        t0, a0, -16
    ld.d        t0, a1, -8
    st.d        t0, a0, -8

    andi        a2, a2, 0x7
    pcaddi      t1, 18
    slli.d      a2, a2, 3
    sub.d       t1, t1, a2
    jr          t1

    ld.b        t0, a4, -7
    st.b        t0, a3, -7
    ld.b        t0, a4, -6
    st.b        t0, a3, -6
    ld.b        t0, a4, -5
    st.b        t0, a3, -5
    ld.b        t0, a4, -4
    st.b        t0, a3, -4
    ld.b        t0, a4, -3
    st.b        t0, a3, -3
    ld.b        t0, a4, -2
    st.b        t0, a3, -2
    ld.b        t0, a4, -1
    st.b        t0, a3, -1
L(end):
    move        a0, t8
    jr          ra

L(all_align):
    addi.d      a1, a1, 0x8
    addi.d      a0, a0, 0x8
    ld.d        t0, a1, -8
    st.d        t0, a0, -8
    addi.d      a2, a2, -8
    b           L(start_over)

L(all_align_back):
    addi.d      a4, a4, -0x8
    addi.d      a3, a3, -0x8
    ld.d        t0, a4, 0
    st.d        t0, a3, 0
    addi.d      a2, a2, -8
    b           L(start_over_back)

L(copy_backward):
    move        a5, a3
    srli.d      a3, a3, 3
    slli.d      a3, a3, 3
    beq         a3, a5, L(all_align_back)
    sub.d       a7, a3, a5
    add.d       a4, a4, a7
    add.d       a2, a7, a2

    pcaddi      t1, 18
    slli.d      a6, a7, 3
    add.d       t1, t1, a6
    jr          t1

    ld.b        t0, a4, 6
    st.b        t0, a3, 6
    ld.b        t0, a4, 5
    st.b        t0, a3, 5
    ld.b        t0, a4, 4
    st.b        t0, a3, 4
    ld.b        t0, a4, 3
    st.b        t0, a3, 3
    ld.b        t0, a4, 2
    st.b        t0, a3, 2
    ld.b        t0, a4, 1
    st.b        t0, a3, 1
    ld.b        t0, a4, 0
    st.b        t0, a3, 0
L(start_over_back):
    addi.d      a2, a2, -0x80
    blt         a2, zero, L(end_unalign_proc_back)

L(loop_less_back):
    LD_64(a4, -64)
    ST_64(a3, -64)
    LD_64(a4, -128)
    ST_64(a3, -128)

    addi.d      a4, a4, -0x80
    addi.d      a3, a3, -0x80
    addi.d      a2, a2, -0x80
    bge         a2, zero, L(loop_less_back)

L(end_unalign_proc_back):
    addi.d      a2, a2, 0x80

    pcaddi      t1, 36
    andi        t2, a2, 0x78
    sub.d       a4, a4, t2
    sub.d       a3, a3, t2
    sub.d       t1, t1, t2
    jr          t1

    ld.d        t0, a4, 112
    st.d        t0, a3, 112
    ld.d        t0, a4, 104
    st.d        t0, a3, 104
    ld.d        t0, a4, 96
    st.d        t0, a3, 96
    ld.d        t0, a4, 88
    st.d        t0, a3, 88
    ld.d        t0, a4, 80
    st.d        t0, a3, 80
    ld.d        t0, a4, 72
    st.d        t0, a3, 72
    ld.d        t0, a4, 64
    st.d        t0, a3, 64
    ld.d        t0, a4, 56
    st.d        t0, a3, 56
    ld.d        t0, a4, 48
    st.d        t0, a3, 48
    ld.d        t0, a4, 40
    st.d        t0, a3, 40
    ld.d        t0, a4, 32
    st.d        t0, a3, 32
    ld.d        t0, a4, 24
    st.d        t0, a3, 24
    ld.d        t0, a4, 16
    st.d        t0, a3, 16
    ld.d        t0, a4, 8
    st.d        t0, a3, 8
    ld.d        t0, a4, 0
    st.d        t0, a3, 0

    andi        a2, a2, 0x7
    pcaddi      t1, 18
    slli.d      a2, a2, 3
    sub.d       t1, t1, a2
    jr          t1

    ld.b        t0, a1, 6
    st.b        t0, a0, 6
    ld.b        t0, a1, 5
    st.b        t0, a0, 5
    ld.b        t0, a1, 4
    st.b        t0, a0, 4
    ld.b        t0, a1, 3
    st.b        t0, a0, 3
    ld.b        t0, a1, 2
    st.b        t0, a0, 2
    ld.b        t0, a1, 1
    st.b        t0, a0, 1
    ld.b        t0, a1, 0
    st.b        t0, a0, 0

    move        a0, t8
    jr          ra
END(MEMMOVE_NAME)

libc_hidden_builtin_def (MEMMOVE_NAME)
#endif
