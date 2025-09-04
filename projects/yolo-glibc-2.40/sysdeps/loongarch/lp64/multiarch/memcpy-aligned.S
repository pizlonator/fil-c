/* Optimized memcpy_aligned implementation using basic LoongArch instructions.
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
# define MEMCPY_NAME __memcpy_aligned
# define MEMMOVE_NAME __memmove_aligned
#else
# define MEMCPY_NAME memcpy
# define MEMMOVE_NAME memmove
#endif

#define LD_64(reg, n)            \
    ld.d        t0, reg, n;      \
    ld.d        t1, reg, n + 8;  \
    ld.d        t2, reg, n + 16; \
    ld.d        t3, reg, n + 24; \
    ld.d        t4, reg, n + 32; \
    ld.d        t5, reg, n + 40; \
    ld.d        t6, reg, n + 48; \
    ld.d        t7, reg, n + 56;

#define ST_64(reg, n)            \
    st.d        t0, reg, n;      \
    st.d        t1, reg, n + 8;  \
    st.d        t2, reg, n + 16; \
    st.d        t3, reg, n + 24; \
    st.d        t4, reg, n + 32; \
    st.d        t5, reg, n + 40; \
    st.d        t6, reg, n + 48; \
    st.d        t7, reg, n + 56;

LEAF(MEMMOVE_NAME, 6)
    sub.d       t0, a0, a1
    bltu        t0, a2, L(copy_back)
END(MEMMOVE_NAME)

LEAF_NO_ALIGN(MEMCPY_NAME)
    srai.d      a3, a2, 4
    beqz        a3, L(short_data)

    move        a4, a0
    andi        a5, a0, 0x7
    andi        a6, a1, 0x7
    li.d        t8, 8
    beqz        a5, L(check_align)

    sub.d       t2, t8, a5
    sub.d       a2, a2, t2
    pcaddi      t1, 20
    slli.d      t3, t2, 3

    add.d       a1, a1, t2
    sub.d       t1, t1, t3
    add.d       a4, a4, t2
    jr          t1

L(al7):
    ld.b        t0, a1, -7
    st.b        t0, a4, -7
L(al6):
    ld.b        t0, a1, -6
    st.b        t0, a4, -6
L(al5):
    ld.b        t0, a1, -5
    st.b        t0, a4, -5
L(al4):
    ld.b        t0, a1, -4
    st.b        t0, a4, -4
L(al3):
    ld.b        t0, a1, -3
    st.b        t0, a4, -3
L(al2):
    ld.b        t0, a1, -2
    st.b        t0, a4, -2
L(al1):
    ld.b        t0, a1, -1
    st.b        t0, a4, -1

L(check_align):
    bne         a5, a6, L(unalign)
    srai.d      a3, a2, 4
    beqz        a3, L(al_less_16bytes)
    andi        a3, a2, 0x3f

    beq         a3, a2, L(al_less_64bytes)
    sub.d       t0, a2, a3
    move        a2, a3
    add.d       a5, a1, t0

L(loop_64bytes):
    LD_64(a1, 0)
    addi.d      a1, a1, 64
    ST_64(a4, 0)

    addi.d      a4, a4, 64
    bne         a1, a5, L(loop_64bytes)

L(al_less_64bytes):
    srai.d     a3, a2, 5
    beqz       a3, L(al_less_32bytes)

    ld.d       t0, a1, 0
    ld.d       t1, a1, 8
    ld.d       t2, a1, 16
    ld.d       t3, a1, 24

    addi.d     a1, a1, 32
    addi.d     a2, a2, -32

    st.d       t0, a4, 0
    st.d       t1, a4, 8
    st.d       t2, a4, 16
    st.d       t3, a4, 24

    addi.d     a4, a4, 32

L(al_less_32bytes):
    srai.d     a3, a2, 4
    beqz       a3, L(al_less_16bytes)

    ld.d       t0, a1, 0
    ld.d       t1, a1, 8
    addi.d     a1, a1, 16
    addi.d     a2, a2, -16

    st.d       t0, a4, 0
    st.d       t1, a4, 8
    addi.d     a4, a4, 16

L(al_less_16bytes):
    srai.d     a3, a2, 3
    beqz       a3, L(al_less_8bytes)

    ld.d       t0, a1, 0
    addi.d     a1, a1, 8
    addi.d     a2, a2, -8
    st.d       t0, a4, 0
    addi.d     a4, a4, 8

L(al_less_8bytes):
    srai.d      a3, a2, 2
    beqz        a3, L(al_less_4bytes)

    ld.w        t0, a1, 0
    addi.d      a1, a1, 4
    addi.d      a2, a2, -4
    st.w        t0, a4, 0
    addi.d      a4, a4, 4

L(al_less_4bytes):
    srai.d      a3, a2, 1
    beqz        a3, L(al_less_2bytes)

    ld.h        t0, a1, 0
    addi.d      a1, a1, 2
    addi.d      a2, a2, -2
    st.h        t0, a4, 0
    addi.d      a4, a4, 2

L(al_less_2bytes):
    beqz        a2, L(al_less_1byte)

    ld.b        t0, a1, 0
    st.b        t0, a4, 0

L(al_less_1byte):
    jr          ra

L(unalign):
    andi        a5, a1, 0x7
    bstrins.d   a1, zero, 2, 0
    sub.d       t8, t8, a5
    slli.d      a5, a5, 3

    ld.d        t0, a1, 0
    addi.d      a1, a1, 8
    slli.d      a6, t8, 3
    srl.d       a7, t0, a5

    srai.d      a3, a2, 4
    beqz        a3, L(un_less_16bytes)
    andi        a3, a2, 0x3f
    beq         a3, a2, L(un_less_64bytes)

    sub.d       t0, a2, a3
    move        a2, a3
    add.d       a3, a1, t0

L(un_long_bytes):
    ld.d        t0, a1, 0
    ld.d        t1, a1, 8
    ld.d        t2, a1, 16
    ld.d        t3, a1, 24

    srl.d       t4, t0, a5
    sll.d       t0, t0, a6
    srl.d       t5, t1, a5
    sll.d       t1, t1, a6

    srl.d       t6, t2, a5
    sll.d       t2, t2, a6
    srl.d       t7, t3, a5
    sll.d       t3, t3, a6

    or          t0, a7, t0
    or          t1, t4, t1
    or          t2, t5, t2
    or          t3, t6, t3

    ld.d        t4, a1, 32
    ld.d        t5, a1, 40
    ld.d        t6, a1, 48
    ld.d        a7, a1, 56

    st.d        t0, a4, 0
    st.d        t1, a4, 8
    st.d        t2, a4, 16
    st.d        t3, a4, 24

    addi.d      a1, a1, 64

    srl.d       t0, t4, a5
    sll.d       t4, t4, a6
    srl.d       t1, t5, a5
    sll.d       t5, t5, a6

    srl.d       t2, t6, a5
    sll.d       t6, t6, a6
    sll.d       t3, a7, a6
    srl.d       a7, a7, a5

    or          t4, t7, t4
    or          t5, t0, t5
    or          t6, t1, t6
    or          t3, t2, t3

    st.d        t4, a4, 32
    st.d        t5, a4, 40
    st.d        t6, a4, 48
    st.d        t3, a4, 56

    addi.d      a4, a4, 64
    bne         a3, a1, L(un_long_bytes)

L(un_less_64bytes):
    srai.d	a3, a2, 5
    beqz	a3, L(un_less_32bytes)

    ld.d        t0, a1, 0
    ld.d        t1, a1, 8
    ld.d        t2, a1, 16
    ld.d        t3, a1, 24

    addi.d      a1, a1, 32
    addi.d      a2, a2, -32

    srl.d       t4, t0, a5
    sll.d       t0, t0, a6
    srl.d       t5, t1, a5
    sll.d       t1, t1, a6

    srl.d       t6, t2, a5
    sll.d       t2, t2, a6
    or          t0, a7, t0
    srl.d       a7, t3, a5
    sll.d       t3, t3, a6

    or          t1, t4, t1
    or          t2, t5, t2
    or          t3, t6, t3

    st.d        t0, a4, 0
    st.d        t1, a4, 8
    st.d        t2, a4, 16
    st.d        t3, a4, 24

    addi.d      a4, a4, 32

L(un_less_32bytes):
    srai.d      a3, a2, 4
    beqz        a3, L(un_less_16bytes)

    ld.d        t0, a1, 0
    ld.d        t1, a1, 8
    addi.d      a1, a1, 16
    addi.d      a2, a2, -16

    srl.d       t2, t0, a5
    sll.d       t3, t0, a6
    sll.d       t4, t1, a6
    or          t3, a7, t3
    or          t4, t2, t4

    srl.d       a7, t1, a5
    st.d        t3, a4, 0
    st.d        t4, a4, 8
    addi.d      a4, a4, 16

L(un_less_16bytes):
    srai.d      a3, a2, 3
    beqz        a3, L(un_less_8bytes)

    ld.d        t0, a1, 0
    addi.d      a1, a1, 8
    addi.d      a2, a2, -8
    sll.d       t1, t0, a6

    or          t2, a7, t1
    srl.d       a7, t0, a5
    st.d        t2, a4, 0
    addi.d      a4, a4, 8

L(un_less_8bytes):
    beqz        a2, L(un_less_1byte)
    bge         t8, a2, 1f

    ld.d        t0, a1, 0
    sll.d       t0, t0, a6
    or          a7, a7, t0

1:
    srai.d      a3, a2, 2
    beqz        a3, L(un_less_4bytes)

    addi.d      a2, a2, -4
    st.w        a7, a4, 0
    addi.d      a4, a4, 4
    srai.d      a7, a7, 32

L(un_less_4bytes):
    srai.d      a3, a2, 1
    beqz        a3, L(un_less_2bytes)

    addi.d      a2, a2, -2
    st.h        a7, a4, 0
    addi.d      a4, a4, 2
    srai.d      a7, a7, 16

L(un_less_2bytes):
    beqz        a2, L(un_less_1byte)
    st.b        a7, a4, 0

L(un_less_1byte):
    jr          ra

L(short_data):
    pcaddi      t1, 36
    slli.d      t2, a2, 3
    add.d       a4, a0, a2
    sub.d       t1, t1, t2
    add.d       a1, a1, a2
    jr          t1

L(short_15_bytes):
    ld.b       t0, a1, -15
    st.b       t0, a4, -15
L(short_14_bytes):
    ld.b       t0, a1, -14
    st.b       t0, a4, -14
L(short_13_bytes):
    ld.b       t0, a1, -13
    st.b       t0, a4, -13
L(short_12_bytes):
    ld.b       t0, a1, -12
    st.b       t0, a4, -12
L(short_11_bytes):
    ld.b       t0, a1, -11
    st.b       t0, a4, -11
L(short_10_bytes):
    ld.b       t0, a1, -10
    st.b       t0, a4, -10
L(short_9_bytes):
    ld.b       t0, a1, -9
    st.b       t0, a4, -9
L(short_8_bytes):
    ld.b       t0, a1, -8
    st.b       t0, a4, -8
L(short_7_bytes):
    ld.b       t0, a1, -7
    st.b       t0, a4, -7
L(short_6_bytes):
    ld.b       t0, a1, -6
    st.b       t0, a4, -6
L(short_5_bytes):
    ld.b       t0, a1, -5
    st.b       t0, a4, -5
L(short_4_bytes):
    ld.b       t0, a1, -4
    st.b       t0, a4, -4
L(short_3_bytes):
    ld.b       t0, a1, -3
    st.b       t0, a4, -3
L(short_2_bytes):
    ld.b       t0, a1, -2
    st.b       t0, a4, -2
L(short_1_bytes):
    ld.b       t0, a1, -1
    st.b       t0, a4, -1
    jr         ra

L(copy_back):
    srai.d      a3, a2, 4
    beqz        a3, L(back_short_data)

    add.d       a4, a0, a2
    add.d       a1, a1, a2

    andi        a5, a4, 0x7
    andi        a6, a1, 0x7
    beqz        a5, L(back_check_align)

    sub.d       a2, a2, a5
    sub.d       a1, a1, a5
    sub.d       a4, a4, a5

    pcaddi      t1, 18
    slli.d      t3, a5, 3
    sub.d       t1, t1, t3
    jr          t1

    ld.b        t0, a1, 6
    st.b        t0, a4, 6
    ld.b        t0, a1, 5
    st.b        t0, a4, 5
    ld.b        t0, a1, 4
    st.b        t0, a4, 4
    ld.b        t0, a1, 3
    st.b        t0, a4, 3
    ld.b        t0, a1, 2
    st.b        t0, a4, 2
    ld.b        t0, a1, 1
    st.b        t0, a4, 1
    ld.b        t0, a1, 0
    st.b        t0, a4, 0

L(back_check_align):
    bne         a5, a6, L(back_unalign)

    srai.d      a3, a2, 4
    beqz        a3, L(back_less_16bytes)

    andi        a3, a2, 0x3f
    beq         a3, a2, L(back_less_64bytes)

    sub.d       t0, a2, a3
    move        a2, a3
    sub.d       a5, a1, t0

L(back_loop_64bytes):
    LD_64(a1, -64)
    addi.d      a1, a1, -64
    ST_64(a4, -64)

    addi.d      a4, a4, -64
    bne         a1, a5, L(back_loop_64bytes)

L(back_less_64bytes):
    srai.d     a3, a2, 5
    beqz       a3, L(back_less_32bytes)

    ld.d       t0, a1, -32
    ld.d       t1, a1, -24
    ld.d       t2, a1, -16
    ld.d       t3, a1, -8

    addi.d     a1, a1, -32
    addi.d     a2, a2, -32

    st.d       t0, a4, -32
    st.d       t1, a4, -24
    st.d       t2, a4, -16
    st.d       t3, a4, -8

    addi.d     a4, a4, -32

L(back_less_32bytes):
    srai.d     a3, a2, 4
    beqz       a3, L(back_less_16bytes)

    ld.d       t0, a1, -16
    ld.d       t1, a1, -8

    addi.d     a2, a2, -16
    addi.d     a1, a1, -16

    st.d       t0, a4, -16
    st.d       t1, a4, -8
    addi.d     a4, a4, -16

L(back_less_16bytes):
    srai.d      a3, a2, 3
    beqz        a3, L(back_less_8bytes)

    ld.d        t0, a1, -8
    addi.d      a2, a2, -8
    addi.d      a1, a1, -8

    st.d        t0, a4, -8
    addi.d      a4, a4, -8

L(back_less_8bytes):
    srai.d      a3, a2, 2
    beqz        a3, L(back_less_4bytes)

    ld.w        t0, a1, -4
    addi.d      a2, a2, -4
    addi.d      a1, a1, -4

    st.w        t0, a4, -4
    addi.d      a4, a4, -4

L(back_less_4bytes):
    srai.d      a3, a2, 1
    beqz        a3, L(back_less_2bytes)

    ld.h        t0, a1, -2
    addi.d      a2, a2, -2
    addi.d      a1, a1, -2

    st.h        t0, a4, -2
    addi.d      a4, a4, -2

L(back_less_2bytes):
    beqz        a2, L(back_less_1byte)

    ld.b        t0, a1, -1
    st.b        t0, a4, -1

L(back_less_1byte):
    jr          ra

L(back_unalign):
    andi        t8, a1, 0x7
    bstrins.d   a1, zero, 2, 0

    sub.d       a6, zero, t8

    ld.d        t0, a1, 0
    slli.d      a6, a6, 3
    slli.d      a5, t8, 3
    sll.d       a7, t0, a6

    srai.d      a3, a2, 4
    beqz        a3, L(back_un_less_16bytes)

    andi        a3, a2, 0x3f
    beq         a3, a2, L(back_un_less_64bytes)

    sub.d       t0, a2, a3
    move        a2, a3
    sub.d       a3, a1, t0

L(back_un_long_bytes):
    ld.d        t0, a1, -8
    ld.d        t1, a1, -16
    ld.d        t2, a1, -24
    ld.d        t3, a1, -32

    sll.d       t4, t0, a6
    srl.d       t0, t0, a5

    sll.d       t5, t1, a6
    srl.d       t1, t1, a5

    sll.d       t6, t2, a6
    srl.d       t2, t2, a5

    sll.d       t7, t3, a6
    srl.d       t3, t3, a5

    or          t0, t0, a7
    or          t1, t1, t4
    or          t2, t2, t5
    or          t3, t3, t6

    ld.d        t4, a1, -40
    ld.d        t5, a1, -48
    ld.d        t6, a1, -56
    ld.d        a7, a1, -64
    st.d        t0, a4, -8
    st.d        t1, a4, -16
    st.d        t2, a4, -24
    st.d        t3, a4, -32

    addi.d      a1, a1, -64

    sll.d       t0, t4, a6
    srl.d       t4, t4, a5

    sll.d       t1, t5, a6
    srl.d       t5, t5, a5

    sll.d       t2, t6, a6
    srl.d       t6, t6, a5

    srl.d       t3, a7, a5
    sll.d       a7, a7, a6

    or          t4, t7, t4
    or          t5, t0, t5
    or          t6, t1, t6
    or          t3, t2, t3

    st.d        t4, a4, -40
    st.d        t5, a4, -48
    st.d        t6, a4, -56
    st.d        t3, a4, -64

    addi.d      a4, a4, -64
    bne         a3, a1, L(back_un_long_bytes)

L(back_un_less_64bytes):
    srai.d	a3, a2, 5
    beqz	a3, L(back_un_less_32bytes)

    ld.d        t0, a1, -8
    ld.d        t1, a1, -16
    ld.d        t2, a1, -24
    ld.d        t3, a1, -32

    addi.d      a1, a1, -32
    addi.d      a2, a2, -32

    sll.d       t4, t0, a6
    srl.d       t0, t0, a5

    sll.d       t5, t1, a6
    srl.d       t1, t1, a5

    sll.d       t6, t2, a6
    srl.d       t2, t2, a5

    or          t0, a7, t0

    sll.d       a7, t3, a6
    srl.d       t3, t3, a5

    or          t1, t4, t1
    or          t2, t5, t2
    or          t3, t6, t3

    st.d        t0, a4, -8
    st.d        t1, a4, -16
    st.d        t2, a4, -24
    st.d        t3, a4, -32

    addi.d      a4, a4, -32

L(back_un_less_32bytes):
    srai.d      a3, a2, 4
    beqz        a3, L(back_un_less_16bytes)

    ld.d        t0, a1, -8
    ld.d        t1, a1, -16

    addi.d      a1, a1, -16
    addi.d      a2, a2, -16

    sll.d       t2, t0, a6
    srl.d       t3, t0, a5

    srl.d       t4, t1, a5
    or          t3, a7, t3
    or          t4, t2, t4
    sll.d       a7, t1, a6

    st.d        t3, a4, -8
    st.d        t4, a4, -16

    addi.d      a4, a4, -16

L(back_un_less_16bytes):
    srai.d      a3, a2, 3
    beqz        a3, L(back_un_less_8bytes)

    ld.d        t0, a1, -8

    addi.d      a1, a1, -8
    addi.d      a2, a2, -8

    srl.d       t1, t0, a5
    or          t2, a7, t1
    sll.d       a7, t0, a6

    st.d        t2, a4, -8
    addi.d      a4, a4, -8

L(back_un_less_8bytes):
    beqz        a2, L(back_end)
    bge         t8, a2, 1f

    ld.d        t0, a1, -8
    srl.d       t0, t0, a5
    or          a7, a7, t0

1:
    srai.d      a3, a2, 2
    beqz        a3, L(back_un_less_4bytes)

    srai.d      t0, a7, 32
    addi.d      a2, a2, -4
    st.w        t0, a4, -4
    addi.d      a4, a4, -4
    slli.d      a7, a7, 32

L(back_un_less_4bytes):
    srai.d      a3, a2, 1
    beqz        a3, L(back_un_less_2bytes)
    srai.d      t0, a7, 48
    addi.d      a2, a2, -2
    st.h        t0, a4, -2
    addi.d      a4, a4, -2
    slli.d      a7, a7, 16
L(back_un_less_2bytes):
    beqz        a2, L(back_un_less_1byte)
    srai.d      t0, a7, 56
    st.b        t0, a4, -1
L(back_un_less_1byte):
    jr          ra

L(back_short_data):
    pcaddi     t1, 34
    slli.d     t2, a2, 3
    sub.d      t1, t1, t2
    jr         t1

    ld.b       t0, a1, 14
    st.b       t0, a0, 14
    ld.b       t0, a1, 13
    st.b       t0, a0, 13
    ld.b       t0, a1, 12
    st.b       t0, a0, 12
    ld.b       t0, a1, 11
    st.b       t0, a0, 11
    ld.b       t0, a1, 10
    st.b       t0, a0, 10
    ld.b       t0, a1, 9
    st.b       t0, a0, 9
    ld.b       t0, a1, 8
    st.b       t0, a0, 8
    ld.b       t0, a1, 7
    st.b       t0, a0, 7
    ld.b       t0, a1, 6
    st.b       t0, a0, 6
    ld.b       t0, a1, 5
    st.b       t0, a0, 5
    ld.b       t0, a1, 4
    st.b       t0, a0, 4
    ld.b       t0, a1, 3
    st.b       t0, a0, 3
    ld.b       t0, a1, 2
    st.b       t0, a0, 2
    ld.b       t0, a1, 1
    st.b       t0, a0, 1
    ld.b       t0, a1, 0
    st.b       t0, a0, 0
L(back_end):
    jr         ra

END(MEMCPY_NAME)

libc_hidden_builtin_def (MEMMOVE_NAME)
libc_hidden_builtin_def (MEMCPY_NAME)
