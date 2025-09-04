/* Optimized strnlen implementation using LoongArch LSX instructions
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

# define STRNLEN __strnlen_lsx

LEAF(STRNLEN, 6)
    beqz            a1, L(ret0)
    andi            t1, a0, 0x1f
    li.d            t3, 33
    sub.d           a2, a0, t1

    vld             vr0, a2, 0
    vld             vr1, a2, 16
    sub.d           t1, t3, t1
    move            a3, a0

    sltu            t1, a1, t1
    vmsknz.b        vr0, vr0
    vmsknz.b        vr1, vr1
    vilvl.h         vr0, vr1, vr0

    movfr2gr.s      t0, fa0
    sra.w           t0, t0, a0
    orn             t1, t1, t0
    bnez            t1, L(end)


    add.d           a4, a0, a1
    move            a0, a2
    addi.d          a4, a4, -1
    bstrins.d       a4, zero, 4, 0

L(loop):
    vld             vr0, a0, 32
    vld             vr1, a0, 48
    addi.d          a0, a0, 32
    beq             a0, a4, L(out)

    vmin.bu         vr2, vr0, vr1
    vsetanyeqz.b    fcc0, vr2
    bceqz           fcc0, L(loop)
L(out):
    vmsknz.b        vr0, vr0

    vmsknz.b        vr1, vr1
    vilvl.h         vr0, vr1, vr0
    movfr2gr.s      t0, fa0
L(end):
    sub.d           a0, a0, a3


    cto.w           t0, t0
    add.d           a0, a0, t0
    sltu            t1, a0, a1
    masknez         t0, a1, t1

    maskeqz         t1, a0, t1
    or              a0, t0, t1
    jr              ra
L(ret0):
    move            a0, zero

    jr              ra
END(STRNLEN)

libc_hidden_builtin_def (STRNLEN)
#endif
