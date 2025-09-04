/* Optimized memchr implementation using LoongArch LASX instructions.
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

# define MEMCHR __memchr_lasx

LEAF(MEMCHR, 6)
    beqz            a2, L(ret0)
    add.d           a3, a0, a2
    andi            t0, a0, 0x3f
    bstrins.d       a0, zero, 5, 0

    xvld            xr0, a0, 0
    xvld            xr1, a0, 32
    li.d            t1, -1
    li.d            t2, 64

    xvreplgr2vr.b   xr2, a1
    sll.d           t3, t1, t0
    sub.d           t2, t2, t0
    xvseq.b         xr0, xr0, xr2

    xvseq.b         xr1, xr1, xr2
    xvmsknz.b       xr0, xr0
    xvmsknz.b       xr1, xr1
    xvpickve.w      xr3, xr0, 4


    xvpickve.w      xr4, xr1, 4
    vilvl.h         vr0, vr3, vr0
    vilvl.h         vr1, vr4, vr1
    vilvl.w         vr0, vr1, vr0

    movfr2gr.d      t0, fa0
    and             t0, t0, t3
    bgeu            t2, a2, L(end)
    bnez            t0, L(found)

    addi.d          a4, a3, -1
    bstrins.d       a4, zero, 5, 0
L(loop):
    xvld            xr0, a0, 64
    xvld            xr1, a0, 96

    addi.d          a0, a0, 64
    xvseq.b         xr0, xr0, xr2
    xvseq.b         xr1, xr1, xr2
    beq             a0, a4, L(out)


    xvmax.bu        xr3, xr0, xr1
    xvseteqz.v      fcc0, xr3
    bcnez           fcc0, L(loop)
    xvmsknz.b       xr0, xr0

    xvmsknz.b       xr1, xr1
    xvpickve.w      xr3, xr0, 4
    xvpickve.w      xr4, xr1, 4
    vilvl.h         vr0, vr3, vr0

    vilvl.h         vr1, vr4, vr1
    vilvl.w         vr0, vr1, vr0
    movfr2gr.d      t0, fa0
L(found):
    ctz.d           t1, t0

    add.d           a0, a0, t1
    jr              ra
L(ret0):
    move            a0, zero
    jr              ra


L(out):
    xvmsknz.b       xr0, xr0
    xvmsknz.b       xr1, xr1
    xvpickve.w      xr3, xr0, 4
    xvpickve.w      xr4, xr1, 4

    vilvl.h         vr0, vr3, vr0
    vilvl.h         vr1, vr4, vr1
    vilvl.w         vr0, vr1, vr0
    movfr2gr.d      t0, fa0

L(end):
    sub.d           t2, zero, a3
    srl.d           t1, t1, t2
    and             t0, t0, t1
    ctz.d           t1, t0

    add.d           a0, a0, t1
    maskeqz         a0, a0, t0
    jr              ra
END(MEMCHR)

libc_hidden_builtin_def (MEMCHR)
#endif
