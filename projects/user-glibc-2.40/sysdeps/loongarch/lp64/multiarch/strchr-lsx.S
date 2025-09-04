/* Optimized strlen implementation using LoongArch LSX instructions.
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
# define STRCHR __strchr_lsx
#endif

LEAF(STRCHR, 6)
    andi            t1, a0, 0xf
    bstrins.d       a0, zero, 3, 0
    vld             vr0, a0, 0
    li.d            t2, -1

    vreplgr2vr.b    vr1, a1
    sll.d           t3, t2, t1
    vxor.v          vr2, vr0, vr1
    vmin.bu         vr0, vr0, vr2

    vmsknz.b        vr0, vr0
    movfr2gr.s      t0, fa0
    ext.w.h         t0, t0
    orn             t0, t0, t3

    beq             t0, t2, L(loop)
L(found):
    cto.w           t0, t0
    add.d           a0, a0, t0
#ifndef AS_STRCHRNUL
    vreplve.b       vr2, vr2, t0
    vpickve2gr.bu   t1, vr2, 0
    masknez         a0, a0, t1
#endif
    jr              ra


L(loop):
    vld             vr0, a0, 16
    addi.d          a0, a0, 16
    vxor.v          vr2, vr0, vr1
    vmin.bu         vr0, vr0, vr2

    vsetanyeqz.b    fcc0, vr0
    bceqz           fcc0, L(loop)
    vmsknz.b        vr0, vr0
    movfr2gr.s      t0, fa0

    b               L(found)
END(STRCHR)

libc_hidden_builtin_def (STRCHR)
#endif
