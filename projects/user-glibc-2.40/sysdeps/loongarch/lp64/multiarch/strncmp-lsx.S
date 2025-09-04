/* Optimized strncmp implementation using LoongArch LSX instructions.
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

# define STRNCMP __strncmp_lsx

LEAF(STRNCMP, 6)
    beqz            a2, L(ret0)
    pcalau12i       t0, %pc_hi20(L(INDEX))
    andi            a3, a0, 0xf
    vld             vr2, t0, %pc_lo12(L(INDEX))

    andi            a4, a1, 0xf
    li.d            t2, 16
    bne             a3, a4, L(unaligned)
    xor             t0, a0, a3

    xor             t1, a1, a4
    vld             vr0, t0, 0
    vld             vr1, t1, 0
    vreplgr2vr.b    vr3, a3


    sub.d           t2, t2, a3
    vadd.b          vr3, vr3, vr2
    vshuf.b         vr0, vr3, vr0, vr3
    vshuf.b         vr1, vr3, vr1, vr3

    vseq.b          vr3, vr0, vr1
    vmin.bu         vr3, vr0, vr3
    bgeu            t2, a2, L(al_early_end)
    vsetanyeqz.b    fcc0, vr3

    bcnez           fcc0, L(al_end)
    add.d           a3, a0, a2
    addi.d          a4, a3, -1
    bstrins.d       a4, zero, 3, 0

    sub.d           a2, a3, a4
L(al_loop):
    vld             vr0, t0, 16
    vld             vr1, t1, 16
    addi.d          t0, t0, 16


    addi.d          t1, t1, 16
    vseq.b          vr3, vr0, vr1
    vmin.bu         vr3, vr0, vr3
    beq             t0, a4, L(al_early_end)

    vsetanyeqz.b    fcc0, vr3
    bceqz           fcc0, L(al_loop)
L(al_end):
    vseqi.b         vr3, vr3, 0
    vfrstpi.b       vr3, vr3, 0

    vshuf.b         vr0, vr0, vr0, vr3
    vshuf.b         vr1, vr1, vr1, vr3
    vpickve2gr.bu   t0, vr0, 0
    vpickve2gr.bu   t1, vr1, 0

    sub.d           a0, t0, t1
    jr              ra
L(al_early_end):
    vreplgr2vr.b    vr4, a2
    vslt.b          vr4, vr2, vr4


    vorn.v          vr3, vr3, vr4
    b               L(al_end)
L(unaligned):
    slt             a5, a3, a4
    xor             t0, a0, a1

    maskeqz         t0, t0, a5
    xor             a0, a0, t0
    xor             a1, a1, t0
    andi            a3, a0, 0xf

    andi            a4, a1, 0xf
    xor             t0, a0, a3
    xor             t1, a1, a4
    vld             vr0, t0, 0

    vld             vr3, t1, 0
    sub.d           t2, t2, a3
    vreplgr2vr.b    vr4, a3
    vreplgr2vr.b    vr5, a4


    vaddi.bu        vr6, vr2, 16
    vsub.b          vr7, vr4, vr5
    vsub.b          vr6, vr6, vr7
    vadd.b          vr4, vr2, vr4

    vshuf.b         vr1, vr3, vr3, vr6
    vshuf.b         vr0, vr7, vr0, vr4
    vshuf.b         vr1, vr7, vr1, vr4
    vseq.b          vr4, vr0, vr1

    vmin.bu         vr4, vr0, vr4
    bgeu            t2, a2, L(un_early_end)
    vsetanyeqz.b    fcc0, vr4
    bcnez           fcc0, L(un_end)

    add.d           a6, a0, a2
    vslt.b          vr5, vr2, vr5
    addi.d          a7, a6, -1
    vor.v           vr3, vr3, vr5


    bstrins.d       a7, zero, 3, 0
    sub.d           a2, a6, a7
L(un_loop):
    vld             vr0, t0, 16
    addi.d          t0, t0, 16

    vsetanyeqz.b    fcc0, vr3
    bcnez           fcc0, L(has_zero)
    beq             t0, a7, L(end_with_len)
    vor.v           vr1, vr3, vr3

    vld             vr3, t1, 16
    addi.d          t1, t1, 16
    vshuf.b         vr1, vr3, vr1, vr6
    vseq.b          vr4, vr0, vr1

    vmin.bu         vr4, vr0, vr4
    vsetanyeqz.b    fcc0, vr4
    bceqz           fcc0, L(un_loop)
L(un_end):
    vseqi.b         vr4, vr4, 0


    vfrstpi.b       vr4, vr4, 0
    vshuf.b         vr0, vr0, vr0, vr4
    vshuf.b         vr1, vr1, vr1, vr4
    vpickve2gr.bu   t0, vr0, 0

    vpickve2gr.bu   t1, vr1, 0
    sub.d           t2, t0, t1
    sub.d           t3, t1, t0
    masknez         t0, t2, a5

    maskeqz         t1, t3, a5
    or              a0, t0, t1
    jr              ra
L(has_zero):
    vshuf.b         vr1, vr3, vr3, vr6

    vseq.b          vr4, vr0, vr1
    vmin.bu         vr4, vr0, vr4
    bne             t0, a7, L(un_end)
L(un_early_end):
    vreplgr2vr.b    vr5, a2

    vslt.b          vr5, vr2, vr5
    vorn.v          vr4, vr4, vr5
    b               L(un_end)
L(end_with_len):
    sub.d           a6, a3, a4

    bgeu            a6, a2, 1f
    vld             vr4, t1, 16
1:
    vshuf.b         vr1, vr4, vr3, vr6
    vseq.b          vr4, vr0, vr1

    vmin.bu         vr4, vr0, vr4
    vreplgr2vr.b    vr5, a2
    vslt.b          vr5, vr2, vr5
    vorn.v          vr4, vr4, vr5

    b               L(un_end)
L(ret0):
    move            a0, zero
    jr              ra
END(STRNCMP)

    .section         .rodata.cst16,"M",@progbits,16
    .align           4
L(INDEX):
    .dword           0x0706050403020100
    .dword           0x0f0e0d0c0b0a0908

libc_hidden_builtin_def (STRNCMP)
#endif
