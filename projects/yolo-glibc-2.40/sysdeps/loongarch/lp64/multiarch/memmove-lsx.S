/* Optimized memmove implementation using LoongArch LSX instructions.
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

#if IS_IN (libc) && !defined __loongarch_soft_float

# define MEMCPY_NAME __memcpy_lsx
# define MEMMOVE_NAME __memmove_lsx

LEAF(MEMCPY_NAME, 6)
    li.d            t6, 16
    add.d           a3, a0, a2
    add.d           a4, a1, a2
    bgeu            t6, a2, L(less_16bytes)

    li.d            t8, 64
    li.d            t7, 32
    bltu            t8, a2, L(copy_long)
    bltu            t7, a2, L(more_32bytes)

    vld             vr0, a1, 0
    vld             vr1, a4, -16
    vst             vr0, a0, 0
    vst             vr1, a3, -16

    jr              ra
L(more_32bytes):
    vld             vr0, a1, 0
    vld             vr1, a1, 16
    vld             vr2, a4, -32


    vld             vr3, a4, -16
    vst             vr0, a0, 0
    vst             vr1, a0, 16
    vst             vr2, a3, -32

    vst             vr3, a3, -16
    jr              ra
L(less_16bytes):
    srli.d          t0, a2, 3
    beqz            t0, L(less_8bytes)

    vldrepl.d       vr0, a1, 0
    vldrepl.d       vr1, a4, -8
    vstelm.d        vr0, a0, 0, 0
    vstelm.d        vr1, a3, -8, 0

    jr              ra
L(less_8bytes):
    srli.d          t0, a2, 2
    beqz            t0, L(less_4bytes)
    vldrepl.w       vr0, a1, 0


    vldrepl.w       vr1, a4, -4
    vstelm.w        vr0, a0, 0, 0
    vstelm.w        vr1, a3, -4, 0
    jr              ra

L(less_4bytes):
    srli.d          t0, a2, 1
    beqz            t0, L(less_2bytes)
    vldrepl.h       vr0, a1, 0
    vldrepl.h       vr1, a4, -2

    vstelm.h        vr0, a0, 0, 0
    vstelm.h        vr1, a3, -2, 0
    jr              ra
L(less_2bytes):
    beqz            a2, L(less_1bytes)

    ld.b            t0, a1, 0
    st.b            t0, a0, 0
L(less_1bytes):
    jr              ra
    nop
END(MEMCPY_NAME)

LEAF(MEMMOVE_NAME, 6)
    li.d            t6, 16
    add.d           a3, a0, a2
    add.d           a4, a1, a2
    bgeu            t6, a2, L(less_16bytes)

    li.d            t8, 64
    li.d            t7, 32
    bltu            t8, a2, L(move_long)
    bltu            t7, a2, L(more_32bytes)

    vld             vr0, a1, 0
    vld             vr1, a4, -16
    vst             vr0, a0, 0
    vst             vr1, a3, -16

    jr              ra
    nop
L(move_long):
    sub.d           t0, a0, a1
    bltu            t0, a2, L(copy_back)


L(copy_long):
    vld             vr2, a1, 0
    andi            t0, a0, 0xf
    sub.d           t0, t6, t0
    add.d           a1, a1, t0

    sub.d           a2, a2, t0
    andi            t1, a1, 0xf
    bnez            t1, L(unaligned)
    vld             vr0, a1, 0

    addi.d          a2, a2, -16
    vst             vr2, a0, 0
    andi            t2, a2, 0x7f
    add.d           a5, a0, t0

    beq             a2, t2, L(al_less_128)
    sub.d           t3, a2, t2
    move            a2, t2
    add.d           a6, a1, t3


L(al_loop):
    vld             vr1, a1, 16
    vld             vr2, a1, 32
    vld             vr3, a1, 48
    vld             vr4, a1, 64

    vld             vr5, a1, 80
    vld             vr6, a1, 96
    vld             vr7, a1, 112
    vst             vr0, a5, 0

    vld             vr0, a1, 128
    addi.d          a1, a1, 128
    vst             vr1, a5, 16
    vst             vr2, a5, 32

    vst             vr3, a5, 48
    vst             vr4, a5, 64
    vst             vr5, a5, 80
    vst             vr6, a5, 96


    vst             vr7, a5, 112
    addi.d          a5, a5, 128
    bne             a1, a6, L(al_loop)
L(al_less_128):
    blt             a2, t8, L(al_less_64)

    vld             vr1, a1, 16
    vld             vr2, a1, 32
    vld             vr3, a1, 48
    addi.d          a2, a2, -64

    vst             vr0, a5, 0
    vld             vr0, a1, 64
    addi.d          a1, a1, 64
    vst             vr1, a5, 16

    vst             vr2, a5, 32
    vst             vr3, a5, 48
    addi.d          a5, a5, 64
L(al_less_64):
    blt             a2, t7, L(al_less_32)


    vld             vr1, a1, 16
    addi.d          a2, a2, -32
    vst             vr0, a5, 0
    vld             vr0, a1, 32

    addi.d          a1, a1, 32
    vst             vr1, a5, 16
    addi.d          a5, a5, 32
L(al_less_32):
    blt             a2, t6, L(al_less_16)

    vst             vr0, a5, 0
    vld             vr0, a1, 16
    addi.d          a5, a5, 16
L(al_less_16):
    vld             vr1, a4, -16

    vst             vr0, a5, 0
    vst             vr1, a3, -16
    jr              ra
    nop


L(unaligned):
    pcalau12i       t2, %pc_hi20(L(INDEX))
    bstrins.d       a1, zero, 3, 0
    vld             vr8, t2, %pc_lo12(L(INDEX))
    vld             vr0, a1, 0

    vld             vr1, a1, 16
    addi.d          a2, a2, -16
    vst             vr2, a0, 0
    add.d           a5, a0, t0

    vreplgr2vr.b    vr9, t1
    andi            t2, a2, 0x7f
    vadd.b          vr9, vr9, vr8
    addi.d          a1, a1, 32


    beq             t2, a2, L(un_less_128)
    sub.d           t3, a2, t2
    move            a2, t2
    add.d           a6, a1, t3

L(un_loop):
    vld             vr2, a1, 0
    vld             vr3, a1, 16
    vld             vr4, a1, 32
    vld             vr5, a1, 48

    vld             vr6, a1, 64
    vld             vr7, a1, 80
    vshuf.b         vr8, vr1, vr0, vr9
    vld             vr0, a1, 96

    vst             vr8, a5, 0
    vshuf.b         vr8, vr2, vr1, vr9
    vld             vr1, a1, 112
    vst             vr8, a5, 16


    addi.d          a1, a1, 128
    vshuf.b         vr2, vr3, vr2, vr9
    vshuf.b         vr3, vr4, vr3, vr9
    vst             vr2, a5, 32

    vshuf.b         vr4, vr5, vr4, vr9
    vst             vr3, a5, 48
    vshuf.b         vr5, vr6, vr5, vr9
    vst             vr4, a5, 64

    vshuf.b         vr6, vr7, vr6, vr9
    vst             vr5, a5, 80
    vshuf.b         vr7, vr0, vr7, vr9
    vst             vr6, a5, 96

    vst             vr7, a5, 112
    addi.d          a5, a5, 128
    bne             a1, a6, L(un_loop)
L(un_less_128):
    blt             a2, t8, L(un_less_64)


    vld             vr2, a1, 0
    vld             vr3, a1, 16
    vshuf.b         vr4, vr1, vr0, vr9
    vld             vr0, a1, 32

    vst             vr4, a5, 0
    addi.d          a2, a2, -64
    vshuf.b         vr4, vr2, vr1, vr9
    vld             vr1, a1, 48

    addi.d          a1, a1, 64
    vst             vr4, a5, 16
    vshuf.b         vr2, vr3, vr2, vr9
    vshuf.b         vr3, vr0, vr3, vr9

    vst             vr2, a5, 32
    vst             vr3, a5, 48
    addi.d          a5, a5, 64
L(un_less_64):
    blt             a2, t7, L(un_less_32)


    vshuf.b         vr3, vr1, vr0, vr9
    vld             vr0, a1, 0
    vst             vr3, a5, 0
    addi.d          a2, a2, -32

    vshuf.b         vr3, vr0, vr1, vr9
    vld             vr1, a1, 16
    addi.d          a1, a1, 32
    vst             vr3, a5, 16

    addi.d          a5, a5, 32
L(un_less_32):
    blt             a2, t6, L(un_less_16)
    vshuf.b         vr2, vr1, vr0, vr9
    vor.v           vr0, vr1, vr1

    vld             vr1, a1, 0
    vst             vr2, a5, 0
    addi.d          a5, a5, 16
L(un_less_16):
    vld             vr2, a4, -16


    vshuf.b         vr0, vr1, vr0, vr9
    vst             vr0, a5, 0
    vst             vr2, a3, -16
    jr              ra

L(copy_back):
    addi.d          t0, a3, -1
    vld             vr2, a4, -16
    andi            t0, t0, 0xf
    addi.d          t0, t0, 1

    sub.d           a4, a4, t0
    sub.d           a2, a2, t0
    andi            t1, a4, 0xf
    bnez            t1, L(back_unaligned)

    vld             vr0, a4, -16
    addi.d          a2, a2, -16
    vst             vr2, a3, -16
    andi            t2, a2, 0x7f


    sub.d           a3, a3, t0
    beq             t2, a2, L(back_al_less_128)
    sub.d           t3, a2, t2
    move            a2, t2

    sub.d           a6, a4, t3
L(back_al_loop):
    vld             vr1, a4, -32
    vld             vr2, a4, -48
    vld             vr3, a4, -64

    vld             vr4, a4, -80
    vld             vr5, a4, -96
    vld             vr6, a4, -112
    vld             vr7, a4, -128

    vst             vr0, a3, -16
    vld             vr0, a4, -144
    addi.d          a4, a4, -128
    vst             vr1, a3, -32


    vst             vr2, a3, -48
    vst             vr3, a3, -64
    vst             vr4, a3, -80
    vst             vr5, a3, -96

    vst             vr6, a3, -112
    vst             vr7, a3, -128
    addi.d          a3, a3, -128
    bne             a4, a6, L(back_al_loop)

L(back_al_less_128):
    blt             a2, t8, L(back_al_less_64)
    vld             vr1, a4, -32
    vld             vr2, a4, -48
    vld             vr3, a4, -64

    addi.d          a2, a2, -64
    vst             vr0, a3, -16
    vld             vr0, a4, -80
    addi.d          a4, a4, -64


    vst             vr1, a3, -32
    vst             vr2, a3, -48
    vst             vr3, a3, -64
    addi.d          a3, a3, -64

L(back_al_less_64):
    blt             a2, t7, L(back_al_less_32)
    vld             vr1, a4, -32
    addi.d          a2, a2, -32
    vst             vr0, a3, -16

    vld             vr0, a4, -48
    vst             vr1, a3, -32
    addi.d          a3, a3, -32
    addi.d          a4, a4, -32

L(back_al_less_32):
    blt             a2, t6, L(back_al_less_16)
    vst             vr0, a3, -16
    vld             vr0, a4, -32
    addi.d          a3, a3, -16


L(back_al_less_16):
    vld             vr1, a1, 0
    vst             vr0, a3, -16
    vst             vr1, a0, 0
    jr              ra

L(back_unaligned):
    pcalau12i       t2, %pc_hi20(L(INDEX))
    bstrins.d       a4, zero, 3, 0
    vld             vr8, t2, %pc_lo12(L(INDEX))
    vld             vr0, a4, 0

    vld             vr1, a4, -16
    addi.d          a2, a2, -16
    vst             vr2, a3, -16
    sub.d           a3, a3, t0


    vreplgr2vr.b    vr9, t1
    andi            t2, a2, 0x7f
    vadd.b          vr9, vr9, vr8
    addi.d          a4, a4, -16

    beq             t2, a2, L(back_un_less_128)
    sub.d           t3, a2, t2
    move            a2, t2
    sub.d           a6, a4, t3

L(back_un_loop):
    vld             vr2, a4, -16
    vld             vr3, a4, -32
    vld             vr4, a4, -48

    vld             vr5, a4, -64
    vld             vr6, a4, -80
    vld             vr7, a4, -96
    vshuf.b         vr8, vr0, vr1, vr9


    vld             vr0, a4, -112
    vst             vr8, a3, -16
    vshuf.b         vr8, vr1, vr2, vr9
    vld             vr1, a4, -128

    vst             vr8, a3, -32
    addi.d          a4, a4, -128
    vshuf.b         vr2, vr2, vr3, vr9
    vshuf.b         vr3, vr3, vr4, vr9

    vst             vr2, a3, -48
    vshuf.b         vr4, vr4, vr5, vr9
    vst             vr3, a3, -64
    vshuf.b         vr5, vr5, vr6, vr9

    vst             vr4, a3, -80
    vshuf.b         vr6, vr6, vr7, vr9
    vst             vr5, a3, -96
    vshuf.b         vr7, vr7, vr0, vr9


    vst             vr6, a3, -112
    vst             vr7, a3, -128
    addi.d          a3, a3, -128
    bne             a4, a6, L(back_un_loop)

L(back_un_less_128):
    blt             a2, t8, L(back_un_less_64)
    vld             vr2, a4, -16
    vld             vr3, a4, -32
    vshuf.b         vr4, vr0, vr1, vr9

    vld             vr0, a4, -48
    vst             vr4, a3, -16
    addi.d          a2, a2, -64
    vshuf.b         vr4, vr1, vr2, vr9

    vld             vr1, a4, -64
    addi.d          a4, a4, -64
    vst             vr4, a3, -32
    vshuf.b         vr2, vr2, vr3, vr9


    vshuf.b         vr3, vr3, vr0, vr9
    vst             vr2, a3, -48
    vst             vr3, a3, -64
    addi.d          a3, a3, -64

L(back_un_less_64):
    blt             a2, t7, L(back_un_less_32)
    vshuf.b         vr3, vr0, vr1, vr9
    vld             vr0, a4, -16
    vst             vr3, a3, -16

    addi.d          a2, a2, -32
    vshuf.b         vr3, vr1, vr0, vr9
    vld             vr1, a4, -32
    addi.d          a4, a4, -32

    vst             vr3, a3, -32
    addi.d          a3, a3, -32
L(back_un_less_32):
    blt             a2, t6, L(back_un_less_16)
    vshuf.b         vr2, vr0, vr1, vr9


    vor.v           vr0, vr1, vr1
    vld             vr1, a4, -16
    vst             vr2, a3, -16
    addi.d          a3, a3, -16

L(back_un_less_16):
    vld             vr2, a1, 0
    vshuf.b         vr0, vr0, vr1, vr9
    vst             vr0, a3, -16
    vst             vr2, a0, 0

    jr              ra
END(MEMMOVE_NAME)

    .section        .rodata.cst16,"M",@progbits,16
    .align          4
L(INDEX):
    .dword          0x0706050403020100
    .dword          0x0f0e0d0c0b0a0908

libc_hidden_builtin_def (MEMCPY_NAME)
libc_hidden_builtin_def (MEMMOVE_NAME)
#endif
