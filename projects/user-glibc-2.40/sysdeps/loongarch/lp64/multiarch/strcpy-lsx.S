/* Optimized strcpy stpcpy implementation using LoongArch LSX instructions.
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
#  define STRCPY __strcpy_lsx
# endif

LEAF(STRCPY, 6)
    pcalau12i       t0, %pc_hi20(L(INDEX))
    andi            a4, a1, 0xf
    vld             vr1, t0, %pc_lo12(L(INDEX))
    move            a2, a0

    beqz            a4, L(load_start)
    xor             t0, a1, a4
    vld             vr0, t0, 0
    vreplgr2vr.b    vr2, a4

    vadd.b          vr2, vr2, vr1
    vshuf.b         vr0, vr2, vr0, vr2
    vsetanyeqz.b    fcc0, vr0
    bcnez           fcc0, L(end)

L(load_start):
    vld             vr0, a1, 0
    li.d            t1, 16
    andi            a3, a2, 0xf
    vsetanyeqz.b    fcc0, vr0


    sub.d           t0, t1, a3
    bcnez           fcc0, L(end)
    add.d           a1, a1, t0
    vst             vr0, a2, 0

    andi            a3, a1, 0xf
    add.d           a2, a2, t0
    bnez            a3, L(unaligned)
    vld             vr0, a1, 0

    vsetanyeqz.b    fcc0, vr0
    bcnez           fcc0, L(al_end)
L(al_loop):
    vst             vr0, a2, 0
    vld             vr0, a1, 16

    addi.d          a2, a2, 16
    addi.d          a1, a1, 16
    vsetanyeqz.b    fcc0, vr0
    bceqz           fcc0, L(al_loop)


L(al_end):
    vmsknz.b        vr1, vr0
    movfr2gr.s      t0, fa1
    cto.w           t0, t0
    add.d           a1, a1, t0

    vld             vr0, a1, -15
# ifdef USE_AS_STPCPY
    add.d           a0, a2, t0
    vst             vr0, a0, -15
# else
    add.d           a2, a2, t0
    vst             vr0, a2, -15
# endif
    jr              ra

L(end):
    vmsknz.b        vr1, vr0
    movfr2gr.s      t0, fa1
    cto.w           t0, t0
    addi.d          t0, t0, 1

L(end_16):
    andi            t1, t0, 16
    beqz            t1, L(end_8)
    vst             vr0, a2, 0
# ifdef USE_AS_STPCPY
    addi.d          a0, a2, 15
# endif
    jr              ra

L(end_8):
    andi            t2, t0, 8
    andi            t3, t0, 4
    andi            t4, t0, 2
    andi            t5, t0, 1

    beqz            t2, L(end_4)
    vstelm.d        vr0, a2, 0, 0
    addi.d          a2, a2, 8
    vbsrl.v         vr0, vr0, 8

L(end_4):
    beqz            t3, L(end_2)
    vstelm.w        vr0, a2, 0, 0
    addi.d          a2, a2, 4
    vbsrl.v         vr0, vr0, 4

L(end_2):
    beqz            t4, L(end_1)
    vstelm.h        vr0, a2, 0, 0
    addi.d          a2, a2, 2
    vbsrl.v         vr0, vr0, 2


L(end_1):
    beqz            t5, L(out)
    vstelm.b        vr0, a2, 0, 0
    addi.d          a2, a2, 1
L(out):
# ifdef USE_AS_STPCPY
    addi.d          a0, a2, -1
# endif
    jr              ra

    .align          4
L(unaligned):
    bstrins.d       a1, zero, 3, 0
    vld             vr2, a1, 0
    vreplgr2vr.b    vr3, a3
    vslt.b          vr4, vr1, vr3

    vor.v           vr0, vr2, vr4
    vsetanyeqz.b    fcc0, vr0
    bcnez           fcc0, L(un_first_end)
    vld             vr0, a1, 16

    vadd.b          vr3, vr3, vr1
    vshuf.b         vr4, vr0, vr2, vr3
    vsetanyeqz.b    fcc0, vr0
    bcnez           fcc0, L(un_end)


    vor.v           vr2, vr0, vr0
    addi.d          a1, a1, 16
L(un_loop):
    vld             vr0, a1, 16
    vst             vr4, a2, 0

    addi.d          a2, a2, 16
    vshuf.b         vr4, vr0, vr2, vr3
    vsetanyeqz.b    fcc0, vr0
    bcnez           fcc0, L(un_end)

    vld             vr2, a1, 32
    vst             vr4, a2, 0
    addi.d          a1, a1, 32
    addi.d          a2, a2, 16

    vshuf.b         vr4, vr2, vr0, vr3
    vsetanyeqz.b    fcc0, vr2
    bceqz           fcc0, L(un_loop)
    vor.v           vr0, vr2, vr2


    addi.d          a1, a1, -16
L(un_end):
    vsetanyeqz.b    fcc0, vr4
    bcnez           fcc0, 1f
    vst             vr4, a2, 0

1:
    vmsknz.b        vr1, vr0
    movfr2gr.s      t0, fa1
    cto.w           t0, t0
    add.d           a1, a1, t0

    vld             vr0, a1, 1
    add.d           a2, a2, t0
    sub.d           a2, a2, a3
    vst             vr0, a2, 1
# ifdef USE_AS_STPCPY
    addi.d          a0, a2, 16
# endif
    jr              ra
L(un_first_end):
    addi.d          a2, a2, -16
    addi.d          a1, a1, -16
    b               1b
END(STRCPY)

    .section        .rodata.cst16,"M",@progbits,16
    .align          4
L(INDEX):
    .dword          0x0706050403020100
    .dword          0x0f0e0d0c0b0a0908

libc_hidden_builtin_def (STRCPY)
#endif
