/* Optimized memrchr implementation using LoongArch LSX instructions.
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

# define MEMRCHR __memrchr_lsx

LEAF(MEMRCHR, 6)
    beqz            a2, L(ret0)
    addi.d          a2, a2, -1
    add.d           a3, a0, a2
    andi            t1, a3, 0x1f

    bstrins.d       a3, zero, 4, 0
    addi.d          t1, t1, 1
    vld             vr0, a3, 0
    vld             vr1, a3, 16

    sub.d           t2, zero, t1
    li.d            t3, -1
    vreplgr2vr.b    vr2, a1
    andi            t4, a0, 0x1f

    srl.d           t2, t3, t2
    vseq.b          vr0, vr0, vr2
    vseq.b          vr1, vr1, vr2
    vmsknz.b        vr0, vr0


    vmsknz.b        vr1, vr1
    vilvl.h         vr0, vr1, vr0
    movfr2gr.s      t0, fa0
    and             t0, t0, t2

    bltu            a2, t1, L(end)
    bnez            t0, L(found)
    bstrins.d       a0, zero, 4, 0
L(loop):
    vld             vr0, a3, -32

    vld             vr1, a3, -16
    addi.d          a3, a3, -32
    vseq.b          vr0, vr0, vr2
    vseq.b          vr1, vr1, vr2

    beq             a0, a3, L(out)
    vmax.bu         vr3, vr0, vr1
    vseteqz.v       fcc0, vr3
    bcnez           fcc0, L(loop)


    vmsknz.b        vr0, vr0
    vmsknz.b        vr1, vr1
    vilvl.h         vr0, vr1, vr0
    movfr2gr.s      t0, fa0

L(found):
    addi.d          a0, a3, 31
    clz.w           t1, t0
    sub.d           a0, a0, t1
    jr              ra

L(out):
    vmsknz.b        vr0, vr0
    vmsknz.b        vr1, vr1
    vilvl.h         vr0, vr1, vr0
    movfr2gr.s      t0, fa0

L(end):
    sll.d           t2, t3, t4
    and             t0, t0, t2
    addi.d          a0, a3, 31
    clz.w           t1, t0


    sub.d           a0, a0, t1
    maskeqz         a0, a0, t0
    jr              ra
L(ret0):
    move            a0, zero

    jr              ra
END(MEMRCHR)

libc_hidden_builtin_def (MEMRCHR)
#endif
