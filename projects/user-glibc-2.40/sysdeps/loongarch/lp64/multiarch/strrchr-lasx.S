/* Optimized strrchr implementation using LoongArch LASX instructions.
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

#define STRRCHR __strrchr_lasx

LEAF(STRRCHR, 6)
    move            a2, a0
    bstrins.d       a0, zero, 5, 0
    xvld            xr0, a0, 0
    xvld            xr1, a0, 32

    li.d            t2, -1
    xvreplgr2vr.b   xr4, a1
    xvmsknz.b       xr2, xr0
    xvmsknz.b       xr3, xr1

    xvpickve.w      xr5, xr2, 4
    xvpickve.w      xr6, xr3, 4
    vilvl.h         vr2, vr5, vr2
    vilvl.h         vr3, vr6, vr3

    vilvl.w         vr2, vr3, vr2
    movfr2gr.d      t0, fa2
    sra.d           t0, t0, a2
    beq             t0, t2, L(find_tail)


    xvseq.b         xr2, xr0, xr4
    xvseq.b         xr3, xr1, xr4
    xvmsknz.b       xr2, xr2
    xvmsknz.b       xr3, xr3

    xvpickve.w      xr4, xr2, 4
    xvpickve.w      xr5, xr3, 4
    vilvl.h         vr2, vr4, vr2
    vilvl.h         vr3, vr5, vr3

    vilvl.w         vr1, vr3, vr2
    slli.d          t3, t2, 1
    movfr2gr.d      t1, fa1
    cto.d           t0, t0

    srl.d           t1, t1, a2
    sll.d           t3, t3, t0
    addi.d          a0, a2, 63
    andn            t1, t1, t3


    clz.d           t0, t1
    sub.d           a0, a0, t0
    maskeqz         a0, a0, t1
    jr              ra

    .align          5
L(find_tail):
    addi.d          a3, a0, 64
L(loop):
    xvld            xr2, a0, 64
    xvld            xr3, a0, 96
    addi.d          a0, a0, 64

    xvmin.bu        xr5, xr2, xr3
    xvsetanyeqz.b   fcc0, xr5
    bceqz           fcc0, L(loop)
    xvmsknz.b       xr5, xr2


    xvmsknz.b       xr6, xr3
    xvpickve.w      xr7, xr5, 4
    xvpickve.w      xr8, xr6, 4
    vilvl.h         vr5, vr7, vr5

    vilvl.h         vr6, vr8, vr6
    xvseq.b         xr2, xr2, xr4
    xvseq.b         xr3, xr3, xr4
    xvmsknz.b       xr2, xr2

    xvmsknz.b       xr3, xr3
    xvpickve.w      xr7, xr2, 4
    xvpickve.w      xr8, xr3, 4
    vilvl.h         vr2, vr7, vr2

    vilvl.h         vr3, vr8, vr3
    vilvl.w         vr5, vr6, vr5
    vilvl.w         vr2, vr3, vr2
    movfr2gr.d      t0, fa5


    movfr2gr.d      t1, fa2
    slli.d          t3, t2, 1
    cto.d           t0, t0
    sll.d           t3, t3, t0

    andn            t1, t1, t3
    beqz            t1, L(find_loop)
    clz.d           t0, t1
    addi.d          a0, a0, 63

    sub.d           a0, a0, t0
    jr              ra
L(find_loop):
    beq             a0, a3, L(find_end)
    xvld            xr2, a0, -64

    xvld            xr3, a0, -32
    addi.d          a0, a0, -64
    xvseq.b         xr2, xr2, xr4
    xvseq.b         xr3, xr3, xr4


    xvmax.bu        xr5, xr2, xr3
    xvseteqz.v      fcc0, xr5
    bcnez           fcc0, L(find_loop)
    xvmsknz.b       xr0, xr2

    xvmsknz.b       xr1, xr3
    xvpickve.w      xr2, xr0, 4
    xvpickve.w      xr3, xr1, 4
    vilvl.h         vr0, vr2, vr0

    vilvl.h         vr1, vr3, vr1
    vilvl.w         vr0, vr1, vr0
    movfr2gr.d      t0, fa0
    addi.d          a0, a0, 63

    clz.d           t0, t0
    sub.d           a0, a0, t0
    jr              ra
    nop


L(find_end):
    xvseq.b         xr2, xr0, xr4
    xvseq.b         xr3, xr1, xr4
    xvmsknz.b       xr2, xr2
    xvmsknz.b       xr3, xr3

    xvpickve.w      xr4, xr2, 4
    xvpickve.w      xr5, xr3, 4
    vilvl.h         vr2, vr4, vr2
    vilvl.h         vr3, vr5, vr3

    vilvl.w         vr1, vr3, vr2
    movfr2gr.d      t1, fa1
    addi.d          a0, a2, 63
    srl.d           t1, t1, a2

    clz.d           t0, t1
    sub.d           a0, a0, t0
    maskeqz         a0, a0, t1
    jr              ra
END(STRRCHR)

libc_hidden_builtin_def(STRRCHR)
#endif
