/* Optimized strchr implementation using LoongArch LASX instructions.
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

#ifndef AS_STRCHRNUL
# define STRCHR __strchr_lasx
#endif

LEAF(STRCHR, 6)
    andi            t1, a0, 0x1f
    bstrins.d       a0, zero, 4, 0
    xvld            xr0, a0, 0
    li.d            t2, -1

    xvreplgr2vr.b   xr1, a1
    sll.d           t1, t2, t1
    xvxor.v         xr2, xr0, xr1
    xvmin.bu        xr0, xr0, xr2

    xvmsknz.b       xr0, xr0
    xvpickve.w      xr3, xr0, 4
    vilvl.h         vr0, vr3, vr0
    movfr2gr.s      t0, fa0

    orn             t0, t0, t1
    bne             t0, t2, L(end)
    addi.d          a0, a0, 32
    nop


L(loop):
    xvld            xr0, a0, 0
    xvxor.v         xr2, xr0, xr1
    xvmin.bu        xr0, xr0, xr2
    xvsetanyeqz.b   fcc0, xr0

    bcnez           fcc0, L(loop_end)
    xvld            xr0, a0, 32
    addi.d          a0, a0, 64
    xvxor.v         xr2, xr0, xr1

    xvmin.bu        xr0, xr0, xr2
    xvsetanyeqz.b   fcc0, xr0
    bceqz           fcc0, L(loop)
    addi.d          a0, a0, -32

L(loop_end):
    xvmsknz.b       xr0, xr0
    xvpickve.w      xr1, xr0, 4
    vilvl.h         vr0, vr1, vr0
    movfr2gr.s      t0, fa0


L(end):
    cto.w           t0, t0
    add.d           a0, a0, t0
#ifndef AS_STRCHRNUL
    vreplgr2vr.b    vr0, t0
    xvpermi.q       xr3, xr2, 1

    vshuf.b         vr0, vr3, vr2, vr0
    vpickve2gr.bu   t0, vr0, 0
    masknez         a0, a0, t0
#endif
    jr              ra

END(STRCHR)

libc_hidden_builtin_def(STRCHR)
#endif
