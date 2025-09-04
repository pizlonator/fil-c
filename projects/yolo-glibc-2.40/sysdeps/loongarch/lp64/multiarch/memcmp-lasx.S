/* Optimized memcmp implementation using LoongArch LASX instructions.
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

# define MEMCMP __memcmp_lasx

LEAF(MEMCMP, 6)
    li.d            t2, 32
    add.d           a3, a0, a2
    add.d           a4, a1, a2
    bgeu            t2, a2, L(less32)

    li.d            t1, 160
    bgeu            a2, t1, L(make_aligned)
L(loop32):
    xvld            xr0, a0, 0
    xvld            xr1, a1, 0

    addi.d          a0, a0, 32
    addi.d          a1, a1, 32
    addi.d          a2, a2, -32
    xvseq.b         xr2, xr0, xr1

    xvsetanyeqz.b   fcc0, xr2
    bcnez           fcc0, L(end)
L(last_bytes):
    bltu            t2, a2, L(loop32)
    xvld            xr0, a3, -32


    xvld            xr1, a4, -32
    xvseq.b         xr2, xr0, xr1
L(end):
    xvmsknz.b       xr2, xr2
    xvpermi.q       xr4, xr0, 1

    xvpickve.w      xr3, xr2, 4
    xvpermi.q       xr5, xr1, 1
    vilvl.h         vr2, vr3, vr2
    movfr2gr.s      t0, fa2

    cto.w           t0, t0
    vreplgr2vr.b    vr2, t0
    vshuf.b         vr0, vr4, vr0, vr2
    vshuf.b         vr1, vr5, vr1, vr2

    vpickve2gr.bu   t0, vr0, 0
    vpickve2gr.bu   t1, vr1, 0
    sub.d           a0, t0, t1
    jr              ra


L(less32):
    srli.d          t0, a2, 4
    beqz            t0, L(less16)
    vld             vr0, a0, 0
    vld             vr1, a1, 0

    vld             vr2, a3, -16
    vld             vr3, a4, -16
L(short_ret):
    vseq.b          vr4, vr0, vr1
    vseq.b          vr5, vr2, vr3

    vmsknz.b        vr4, vr4
    vmsknz.b        vr5, vr5
    vilvl.h         vr4, vr5, vr4
    movfr2gr.s      t0, fa4

    cto.w           t0, t0
    vreplgr2vr.b    vr4, t0
    vshuf.b         vr0, vr2, vr0, vr4
    vshuf.b         vr1, vr3, vr1, vr4


    vpickve2gr.bu   t0, vr0, 0
    vpickve2gr.bu   t1, vr1, 0
    sub.d           a0, t0, t1
    jr              ra

L(less16):
    srli.d          t0, a2, 3
    beqz            t0, L(less8)
    vldrepl.d       vr0, a0, 0
    vldrepl.d       vr1, a1, 0

    vldrepl.d       vr2, a3, -8
    vldrepl.d       vr3, a4, -8
    b               L(short_ret)
    nop

L(less8):
    srli.d          t0, a2, 2
    beqz            t0, L(less4)
    vldrepl.w       vr0, a0, 0
    vldrepl.w       vr1, a1, 0


    vldrepl.w       vr2, a3, -4
    vldrepl.w       vr3, a4, -4
    b               L(short_ret)
    nop

L(less4):
    srli.d          t0, a2, 1
    beqz            t0, L(less2)
    vldrepl.h       vr0, a0, 0
    vldrepl.h       vr1, a1, 0

    vldrepl.h       vr2, a3, -2
    vldrepl.h       vr3, a4, -2
    b               L(short_ret)
    nop

L(less2):
    beqz            a2, L(ret0)
    ld.bu           t0, a0, 0
    ld.bu           t1, a1, 0
    sub.d           a0, t0, t1

    jr              ra
L(ret0):
    move            a0, zero
    jr              ra

L(make_aligned):
    xvld            xr0, a0, 0

    xvld            xr1, a1, 0
    xvseq.b         xr2, xr0, xr1
    xvsetanyeqz.b   fcc0, xr2
    bcnez           fcc0, L(end)

    andi            t0, a0, 0x1f
    sub.d           t0, t2, t0
    sub.d           t1, a2, t0
    add.d           a0, a0, t0

    add.d           a1, a1, t0
    andi            a2, t1, 0x3f
    sub.d           t0, t1, a2
    add.d           a5, a0, t0


L(loop_align):
    xvld            xr0, a0, 0
    xvld            xr1, a1, 0
    xvld            xr2, a0, 32
    xvld            xr3, a1, 32

    xvseq.b         xr0, xr0, xr1
    xvseq.b         xr1, xr2, xr3
    xvmin.bu        xr2, xr1, xr0
    xvsetanyeqz.b   fcc0, xr2

    bcnez           fcc0, L(pair_end)
    addi.d          a0, a0, 64
    addi.d          a1, a1, 64
    bne             a0, a5, L(loop_align)

    bnez            a2, L(last_bytes)
    move            a0, zero
    jr              ra
    nop


L(pair_end):
    xvmsknz.b       xr0, xr0
    xvmsknz.b       xr1, xr1
    xvpickve.w      xr2, xr0, 4
    xvpickve.w      xr3, xr1, 4

    vilvl.h         vr0, vr2, vr0
    vilvl.h         vr1, vr3, vr1
    vilvl.w         vr0, vr1, vr0
    movfr2gr.d      t0, fa0

    cto.d           t0, t0
    ldx.bu          t1, a0, t0
    ldx.bu          t2, a1, t0
    sub.d           a0, t1, t2

    jr              ra
END(MEMCMP)

libc_hidden_builtin_def (MEMCMP)
#endif
