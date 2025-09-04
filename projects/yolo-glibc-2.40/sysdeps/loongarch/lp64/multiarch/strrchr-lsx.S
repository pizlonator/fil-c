/* Optimized strrchr implementation using LoongArch LSX instructions.
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

#define STRRCHR __strrchr_lsx

LEAF(STRRCHR, 6)
    move            a2, a0
    bstrins.d       a0, zero, 4, 0
    vld             vr0, a0, 0
    vld             vr1, a0, 16

    li.d            t2, -1
    vreplgr2vr.b    vr4, a1
    vmsknz.b        vr2, vr0
    vmsknz.b        vr3, vr1

    vilvl.h         vr2, vr3, vr2
    movfr2gr.s      t0, fa2
    sra.w           t0, t0, a2
    beq             t0, t2, L(find_tail)

    vseq.b          vr2, vr0, vr4
    vseq.b          vr3, vr1, vr4
    vmsknz.b        vr2, vr2
    vmsknz.b        vr3, vr3


    vilvl.h         vr1, vr3, vr2
    slli.d          t3, t2, 1
    movfr2gr.s      t1, fa1
    cto.w           t0, t0

    srl.w           t1, t1, a2
    sll.d           t3, t3, t0
    addi.d          a0, a2, 31
    andn            t1, t1, t3

    clz.w           t0, t1
    sub.d           a0, a0, t0
    maskeqz         a0, a0, t1
    jr              ra

    .align          5
L(find_tail):
    addi.d          a3, a0, 32
L(loop):
    vld             vr2, a0, 32
    vld             vr3, a0, 48
    addi.d          a0, a0, 32

    vmin.bu         vr5, vr2, vr3
    vsetanyeqz.b    fcc0, vr5
    bceqz           fcc0, L(loop)
    vmsknz.b        vr5, vr2

    vmsknz.b        vr6, vr3
    vilvl.h         vr5, vr6, vr5
    vseq.b          vr2, vr2, vr4
    vseq.b          vr3, vr3, vr4

    vmsknz.b        vr2, vr2
    vmsknz.b        vr3, vr3
    vilvl.h         vr2, vr3, vr2
    movfr2gr.s      t0, fa5


    movfr2gr.s      t1, fa2
    slli.d          t3, t2, 1
    cto.w           t0, t0
    sll.d           t3, t3, t0

    andn            t1, t1, t3
    beqz            t1, L(find_loop)
    clz.w           t0, t1
    addi.d          a0, a0, 31

    sub.d           a0, a0, t0
    jr              ra
L(find_loop):
    beq             a0, a3, L(find_end)
    vld             vr2, a0, -32

    vld             vr3, a0, -16
    addi.d          a0, a0, -32
    vseq.b          vr2, vr2, vr4
    vseq.b          vr3, vr3, vr4


    vmax.bu         vr5, vr2, vr3
    vseteqz.v       fcc0, vr5
    bcnez           fcc0, L(find_loop)
    vmsknz.b        vr0, vr2

    vmsknz.b        vr1, vr3
    vilvl.h         vr0, vr1, vr0
    movfr2gr.s      t0, fa0
    addi.d          a0, a0, 31

    clz.w           t0, t0
    sub.d           a0, a0, t0
    jr              ra
    nop

L(find_end):
    vseq.b          vr2, vr0, vr4
    vseq.b          vr3, vr1, vr4
    vmsknz.b        vr2, vr2
    vmsknz.b        vr3, vr3


    vilvl.h         vr1, vr3, vr2
    movfr2gr.s      t1, fa1
    addi.d          a0, a2, 31
    srl.w           t1, t1, a2

    clz.w           t0, t1
    sub.d           a0, a0, t0
    maskeqz         a0, a0, t1
    jr              ra
END(STRRCHR)

libc_hidden_builtin_def(STRRCHR)
#endif
