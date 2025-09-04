/* Optimized memmove implementation using LoongArch LASX instructions.
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

#ifndef MEMCPY_NAME
# define MEMCPY_NAME __memcpy_lasx
#endif

#ifndef MEMMOVE_NAME
# define MEMMOVE_NAME __memmove_lasx
#endif

LEAF(MEMCPY_NAME, 6)
    li.d            t0, 32
    add.d           a3, a0, a2
    add.d           a4, a1, a2
    bgeu            t0, a2, L(less_32bytes)

    li.d            t1, 64
    bltu            t1, a2, L(copy_long)
    xvld            xr0, a1, 0
    xvld            xr1, a4, -32

    xvst            xr0, a0, 0
    xvst            xr1, a3, -32
    jr              ra
L(less_32bytes):
    srli.d          t0, a2, 4

    beqz            t0, L(less_16bytes)
    vld             vr0, a1, 0
    vld             vr1, a4, -16
    vst             vr0, a0, 0


    vst             vr1, a3, -16
    jr              ra
L(less_16bytes):
    srli.d          t0, a2, 3
    beqz            t0, L(less_8bytes)

    ld.d            t0, a1, 0
    ld.d            t1, a4, -8
    st.d            t0, a0, 0
    st.d            t1, a3, -8

    jr              ra
L(less_8bytes):
    srli.d          t0, a2, 2
    beqz            t0, L(less_4bytes)
    ld.w            t0, a1, 0

    ld.w            t1, a4, -4
    st.w            t0, a0, 0
    st.w            t1, a3, -4
    jr              ra


L(less_4bytes):
    srli.d          t0, a2, 1
    beqz            t0, L(less_2bytes)
    ld.h            t0, a1, 0
    ld.h            t1, a4, -2

    st.h            t0, a0, 0
    st.h            t1, a3, -2
    jr              ra
L(less_2bytes):
    beqz            a2, L(less_1bytes)

    ld.b            t0, a1, 0
    st.b            t0, a0, 0
L(less_1bytes):
    jr              ra
END(MEMCPY_NAME)

LEAF(MEMMOVE_NAME, 6)

    li.d            t0, 32
    add.d           a3, a0, a2
    add.d           a4, a1, a2
    bgeu            t0, a2, L(less_32bytes)

    li.d            t1, 64
    bltu            t1, a2, L(move_long)
    xvld            xr0, a1, 0
    xvld            xr1, a4, -32

    xvst            xr0, a0, 0
    xvst            xr1, a3, -32
    jr              ra
L(move_long):
    sub.d           t2, a0, a1

    bltu            t2, a2, L(copy_back)
L(copy_long):
    andi            t2, a0, 0x1f
    addi.d          a2, a2, -1
    sub.d           t2, t0, t2


    xvld            xr8, a1, 0
    xvld            xr9, a4, -32
    sub.d           t3, a2, t2
    add.d           a5, a0, t2

    andi            a2, t3, 0xff
    add.d           a1, a1, t2
    beq             a2, t3, L(lt256)
    sub.d           a6, a4, a2

    addi.d          a6, a6, -1
L(loop_256):
    xvld            xr0, a1, 0
    xvld            xr1, a1, 32
    xvld            xr2, a1, 64

    xvld            xr3, a1, 96
    xvld            xr4, a1, 128
    xvld            xr5, a1, 160
    xvld            xr6, a1, 192


    xvld            xr7, a1, 224
    addi.d          a1, a1, 256
    xvst            xr0, a5, 0
    xvst            xr1, a5, 32

    xvst            xr2, a5, 64
    xvst            xr3, a5, 96
    xvst            xr4, a5, 128
    xvst            xr5, a5, 160

    xvst            xr6, a5, 192
    xvst            xr7, a5, 224
    addi.d          a5, a5, 256
    bne             a1, a6, L(loop_256)

L(lt256):
    srli.d          t2, a2, 7
    beqz            t2, L(lt128)
    xvld            xr0, a1, 0
    xvld            xr1, a1, 32


    xvld            xr2, a1, 64
    xvld            xr3, a1, 96
    addi.d          a1, a1, 128
    addi.d          a2, a2, -128

    xvst            xr0, a5, 0
    xvst            xr1, a5, 32
    xvst            xr2, a5, 64
    xvst            xr3, a5, 96

    addi.d          a5, a5, 128
L(lt128):
    bltu            a2, t1, L(lt64)
    xvld            xr0, a1, 0
    xvld            xr1, a1, 32

    addi.d          a1, a1, 64
    addi.d          a2, a2, -64
    xvst            xr0, a5, 0
    xvst            xr1, a5, 32


    addi.d          a5, a5, 64
L(lt64):
    bltu            a2, t0, L(lt32)
    xvld            xr0, a1, 0
    xvst            xr0, a5, 0

L(lt32):
    xvst            xr8, a0, 0
    xvst            xr9, a3, -32
    jr              ra
    nop

L(copy_back):
    addi.d          a3, a3, -1
    addi.d          a2, a2, -2
    andi            t2, a3, 0x1f
    xvld            xr8, a1, 0

    xvld            xr9, a4, -32
    sub.d           t3, a2, t2
    sub.d           a5, a3, t2
    sub.d           a4, a4, t2


    andi            a2, t3, 0xff
    beq             a2, t3, L(back_lt256)
    add.d           a6, a1, a2
    addi.d          a6, a6, 2

L(back_loop_256):
    xvld            xr0, a4, -33
    xvld            xr1, a4, -65
    xvld            xr2, a4, -97
    xvld            xr3, a4, -129

    xvld            xr4, a4, -161
    xvld            xr5, a4, -193
    xvld            xr6, a4, -225
    xvld            xr7, a4, -257

    addi.d          a4, a4, -256
    xvst            xr0, a5, -32
    xvst            xr1, a5, -64
    xvst            xr2, a5, -96


    xvst            xr3, a5, -128
    xvst            xr4, a5, -160
    xvst            xr5, a5, -192
    xvst            xr6, a5, -224

    xvst            xr7, a5, -256
    addi.d          a5, a5, -256
    bne             a4, a6, L(back_loop_256)
L(back_lt256):
    srli.d          t2, a2, 7

    beqz            t2, L(back_lt128)
    xvld            xr0, a4, -33
    xvld            xr1, a4, -65
    xvld            xr2, a4, -97

    xvld            xr3, a4, -129
    addi.d          a2, a2, -128
    addi.d          a4, a4, -128
    xvst            xr0, a5, -32


    xvst            xr1, a5, -64
    xvst            xr2, a5, -96
    xvst            xr3, a5, -128
    addi.d          a5, a5, -128

L(back_lt128):
    blt             a2, t1, L(back_lt64)
    xvld            xr0, a4, -33
    xvld            xr1, a4, -65
    addi.d          a2, a2, -64

    addi.d          a4, a4, -64
    xvst            xr0, a5, -32
    xvst            xr1, a5, -64
    addi.d          a5, a5, -64

L(back_lt64):
    bltu            a2, t0, L(back_lt32)
    xvld            xr0, a4, -33
    xvst            xr0, a5, -32
L(back_lt32):
    xvst            xr8, a0, 0


    xvst            xr9, a3, -31
    jr              ra
END(MEMMOVE_NAME)

libc_hidden_builtin_def (MEMCPY_NAME)
libc_hidden_builtin_def (MEMMOVE_NAME)
#endif
