/* Optimized strlen implementation using LoongArch LASX instructions.
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

# define STRLEN __strlen_lasx

LEAF(STRLEN, 6)
    move            a1, a0
    bstrins.d       a0, zero, 4, 0
    li.d            t1, -1
    xvld            xr0, a0, 0

    xvmsknz.b       xr0, xr0
    xvpickve.w      xr1, xr0, 4
    vilvl.h         vr0, vr1, vr0
    movfr2gr.s      t0, fa0  # sign extend

    sra.w           t0, t0, a1
    beq             t0, t1, L(loop)
    cto.w           a0, t0
    jr              ra

L(loop):
    xvld            xr0, a0, 32
    addi.d          a0, a0, 32
    xvsetanyeqz.b   fcc0, xr0
    bceqz           fcc0, L(loop)


    xvmsknz.b       xr0, xr0
    sub.d           a0, a0, a1
    xvpickve.w      xr1, xr0, 4
    vilvl.h         vr0, vr1, vr0

    movfr2gr.s      t0, fa0
    cto.w           t0, t0
    add.d           a0, a0, t0
    jr              ra
END(STRLEN)

libc_hidden_builtin_def (STRLEN)
#endif
