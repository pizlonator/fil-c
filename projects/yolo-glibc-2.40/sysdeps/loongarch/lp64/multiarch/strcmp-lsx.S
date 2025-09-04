/* Optimized strcmp implementation using LoongArch LSX instructions.
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

# define STRCMP	__strcmp_lsx

LEAF(STRCMP, 6)
    pcalau12i       t0, %pc_hi20(L(INDEX))
    andi            a2, a0, 0xf
    vld             vr2, t0, %pc_lo12(L(INDEX))
    andi            a3, a1, 0xf

    bne             a2, a3, L(unaligned)
    bstrins.d       a0, zero, 3, 0
    bstrins.d       a1, zero, 3, 0
    vld             vr0, a0, 0

    vld             vr1, a1, 0
    vreplgr2vr.b    vr3, a2
    vslt.b          vr2, vr2, vr3
    vseq.b          vr3, vr0, vr1

    vmin.bu         vr3, vr0, vr3
    vor.v           vr3, vr3, vr2
    vsetanyeqz.b    fcc0, vr3
    bcnez           fcc0, L(al_out)


L(al_loop):
    vld             vr0, a0, 16
    vld             vr1, a1, 16
    addi.d          a0, a0, 16
    addi.d          a1, a1, 16

    vseq.b          vr3, vr0, vr1
    vmin.bu         vr3, vr0, vr3
    vsetanyeqz.b    fcc0, vr3
    bceqz           fcc0, L(al_loop)

L(al_out):
    vseqi.b         vr3, vr3, 0
    vfrstpi.b       vr3, vr3, 0
    vshuf.b         vr0, vr0, vr0, vr3
    vshuf.b         vr1, vr1, vr1, vr3

    vpickve2gr.bu   t0, vr0, 0
    vpickve2gr.bu   t1, vr1, 0
    sub.d           a0, t0, t1
    jr              ra


L(unaligned):
    slt             a4, a3, a2
    xor             t0, a0, a1
    maskeqz         t0, t0, a4
    xor             a0, a0, t0

    xor             a1, a1, t0
    andi            a2, a0, 0xf
    andi            a3, a1, 0xf
    bstrins.d       a0, zero, 3, 0

    bstrins.d       a1, zero, 3, 0
    vld             vr3, a0, 0
    vld             vr1, a1, 0
    vreplgr2vr.b    vr4, a2

    vreplgr2vr.b    vr5, a3
    vslt.b          vr7, vr2, vr5
    vsub.b          vr5, vr5, vr4
    vaddi.bu        vr6, vr2, 16


    vsub.b          vr6, vr6, vr5
    vshuf.b         vr0, vr3, vr3, vr6
    vor.v           vr0, vr0, vr7
    vor.v           vr1, vr1, vr7

    vseq.b          vr5, vr0, vr1
    vsetanyeqz.b    fcc0, vr5
    bcnez           fcc0, L(not_equal)
    vslt.b          vr4, vr2, vr4

    vor.v           vr0, vr3, vr4
    vsetanyeqz.b    fcc0, vr0
    bcnez           fcc0, L(find_zero)
    nop

L(un_loop):
    vld             vr3, a0, 16
    vld             vr1, a1, 16
    addi.d          a0, a0, 16
    addi.d          a1, a1, 16


    vshuf.b         vr0, vr3, vr0, vr6
    vseq.b          vr5, vr0, vr1
    vsetanyeqz.b    fcc0, vr5
    bcnez           fcc0, L(not_equal)

    vsetanyeqz.b    fcc0, vr3
    vor.v           vr0, vr3, vr3
    bceqz           fcc0, L(un_loop)
L(find_zero):
    vmin.bu         vr5, vr1, vr5

    vsetanyeqz.b    fcc0, vr5
    bcnez           fcc0, L(ret0)
    vld             vr1, a1, 16
    vshuf.b         vr0, vr3, vr3, vr6

    vseq.b          vr5, vr0, vr1
L(not_equal):
    vmin.bu         vr5, vr0, vr5
L(un_end):
    vseqi.b         vr5, vr5, 0
    vfrstpi.b       vr5, vr5, 0


    vshuf.b         vr0, vr0, vr0, vr5
    vshuf.b         vr1, vr1, vr1, vr5
    vpickve2gr.bu   t0, vr0, 0
    vpickve2gr.bu   t1, vr1, 0

    sub.d           t3, t0, t1
    sub.d           t4, t1, t0
    masknez         t0, t3, a4
    maskeqz         t1, t4, a4

    or              a0, t0, t1
    jr              ra
L(ret0):
    move            a0, zero
    jr              ra
END(STRCMP)

    .section         .rodata.cst16,"M",@progbits,16
    .align           4
L(INDEX):
    .dword           0x0706050403020100
    .dword           0x0f0e0d0c0b0a0908

libc_hidden_builtin_def (STRCMP)
#endif
