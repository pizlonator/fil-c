/* Optimized strcpy stpcpy implementation using LoongArch LASX instructions.
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

# ifndef STRCPY
#  define STRCPY __strcpy_lasx
# endif

# ifdef USE_AS_STPCPY
#  define dstend a0
# else
#  define dstend a4
# endif

LEAF(STRCPY, 6)
    ori             t8, zero, 0xfe0
    andi            t0, a1, 0xfff
    li.d            t7, -1
    move            a2, a0

    bltu            t8, t0, L(page_cross_start)
L(start_entry):
    xvld            xr0, a1, 0
    li.d            t0, 32
    andi            t1, a2, 0x1f

    xvsetanyeqz.b   fcc0, xr0
    sub.d           t0, t0, t1
    bcnez           fcc0, L(end)
    add.d           a1, a1, t0

    xvst            xr0, a2, 0
    andi            a3, a1, 0x1f
    add.d           a2, a2, t0
    bnez            a3, L(unaligned)


    xvld            xr0, a1, 0
    xvsetanyeqz.b   fcc0, xr0
    bcnez           fcc0, L(al_end)
L(al_loop):
    xvst            xr0, a2, 0

    xvld            xr0, a1, 32
    addi.d          a2, a2, 32
    addi.d          a1, a1, 32
    xvsetanyeqz.b   fcc0, xr0

    bceqz           fcc0, L(al_loop)
L(al_end):
    xvmsknz.b       xr0, xr0
    xvpickve.w      xr1, xr0, 4
    vilvl.h         vr0, vr1, vr0

    movfr2gr.s      t0, fa0
    cto.w           t0, t0
    add.d           a1, a1, t0
    xvld            xr0, a1, -31


    add.d           dstend, a2, t0
    xvst            xr0, dstend, -31
    jr              ra
    nop

L(page_cross_start):
    move            a4, a1
    bstrins.d       a4, zero, 4, 0
    xvld            xr0, a4, 0
    xvmsknz.b       xr0, xr0

    xvpickve.w      xr1, xr0, 4
    vilvl.h         vr0, vr1, vr0
    movfr2gr.s      t0, fa0
    sra.w           t0, t0, a1

    beq             t0, t7, L(start_entry)
    b               L(tail)
L(unaligned):
    andi            t0, a1, 0xfff
    bltu            t8, t0, L(un_page_cross)


L(un_start_entry):
    xvld            xr0, a1, 0
    xvsetanyeqz.b   fcc0, xr0
    bcnez           fcc0, L(un_end)
    addi.d          a1, a1, 32

L(un_loop):
    xvst            xr0, a2, 0
    andi            t0, a1, 0xfff
    addi.d          a2, a2, 32
    bltu            t8, t0, L(page_cross_loop)

L(un_loop_entry):
    xvld            xr0, a1, 0
    addi.d          a1, a1, 32
    xvsetanyeqz.b   fcc0, xr0
    bceqz           fcc0, L(un_loop)

    addi.d          a1, a1, -32
L(un_end):
    xvmsknz.b       xr0, xr0
    xvpickve.w      xr1, xr0, 4
    vilvl.h         vr0, vr1, vr0


    movfr2gr.s      t0, fa0
L(un_tail):
    cto.w           t0, t0
    add.d           a1, a1, t0
    xvld            xr0, a1, -31

    add.d           dstend, a2, t0
    xvst            xr0, dstend, -31
    jr              ra
L(un_page_cross):
    sub.d           a4, a1, a3

    xvld            xr0, a4, 0
    xvmsknz.b       xr0, xr0
    xvpickve.w      xr1, xr0, 4
    vilvl.h         vr0, vr1, vr0

    movfr2gr.s      t0, fa0
    sra.w           t0, t0, a1
    beq             t0, t7, L(un_start_entry)
    b               L(un_tail)


L(page_cross_loop):
    sub.d           a4, a1, a3
    xvld            xr0, a4, 0
    xvmsknz.b       xr0, xr0
    xvpickve.w      xr1, xr0, 4

    vilvl.h         vr0, vr1, vr0
    movfr2gr.s      t0, fa0
    sra.w           t0, t0, a1
    beq             t0, t7, L(un_loop_entry)

    b               L(un_tail)
L(end):
    xvmsknz.b       xr0, xr0
    xvpickve.w      xr1, xr0, 4
    vilvl.h         vr0, vr1, vr0

    movfr2gr.s      t0, fa0
L(tail):
    cto.w           t0, t0
    add.d           dstend, a2, t0
    add.d           a5, a1, t0

L(less_32):
    srli.d          t1, t0, 4
    beqz            t1, L(less_16)
    vld             vr0, a1, 0
    vld             vr1, a5, -15

    vst             vr0, a2, 0
    vst             vr1, dstend, -15
    jr              ra
L(less_16):
    srli.d          t1, t0, 3

    beqz            t1, L(less_8)
    ld.d            t2, a1, 0
    ld.d            t3, a5, -7
    st.d            t2, a2, 0

    st.d            t3, dstend, -7
    jr              ra
L(less_8):
    li.d            t1, 3
    bltu            t0, t1, L(less_3)

    ld.w            t2, a1, 0
    ld.w            t3, a5, -3
    st.w            t2, a2, 0
    st.w            t3, dstend, -3

    jr              ra
L(less_3):
    beqz            t0, L(zero_byte)
    ld.h            t2, a1, 0

    st.h            t2, a2, 0
L(zero_byte):
    st.b            zero, dstend, 0
    jr              ra
END(STRCPY)

libc_hidden_builtin_def (STRCPY)
#endif
