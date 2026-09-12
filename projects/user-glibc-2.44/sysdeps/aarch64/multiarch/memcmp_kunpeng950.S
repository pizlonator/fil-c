/* Optimized memcmp for Huawei Kunpeng 950 processor.
   Copyright (C) 2026 Free Software Foundation, Inc.

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

/* Assumptions:
 *
 * ARMv8.2-a, AArch64, Advanced SIMD, SVE, unaligned accesses
 *
 */

.arch armv8.2-a+sve

#define src1    x0
#define src2    x1
#define cnt     x2
#define result  w0
#define off_vl      x3
#define off_vlx2    x4
#define off_vlx3    x5
#define off_vlx4    x6

ENTRY (__memcmp_kunpeng950)
    whilelo p0.b, xzr, cnt
    b.none  L(equal)
    cntb    off_vl
    ld1b    z0.b, p0/z, [src1]
    ld1b    z1.b, p0/z, [src2]
    cmpne   p1.b, p0/z, z0.b, z1.b
    b.any   L(mismatch)

    whilelo p0.b, off_vl, cnt
    b.none  L(equal)
    cntb    off_vlx2, all, mul #2
    ld1b    z0.b, p0/z, [src1, 1, mul vl]
    ld1b    z1.b, p0/z, [src2, 1, mul vl]
    cmpne   p1.b, p0/z, z0.b, z1.b
    b.any   L(mismatch)

    whilelo p0.b, off_vlx2, cnt
    b.none  L(equal)
    cntb    off_vlx3, all, mul #3
    ld1b    z0.b, p0/z, [src1, 2, mul vl]
    ld1b    z1.b, p0/z, [src2, 2, mul vl]
    cmpne   p1.b, p0/z, z0.b, z1.b
    b.any   L(mismatch)

    whilelo p0.b, off_vlx3, cnt
    b.none  L(equal)
    cntb    off_vlx4, all, mul #4
    ld1b    z0.b, p0/z, [src1, 3, mul vl]
    ld1b    z1.b, p0/z, [src2, 3, mul vl]
    cmpne   p1.b, p0/z, z0.b, z1.b
    b.any   L(mismatch)

    subs    cnt, cnt, off_vlx4
    b.ls    L(equal)
    add     src1, src1, off_vlx4
    add     src2, src2, off_vlx4
    cmp     cnt, off_vlx4
    b.lo    L(tail_4xvl)

    .p2align 4
L(loop_full):
    ld1b    z0.b, p0/z, [src1]
    ld1b    z1.b, p0/z, [src2]
    cmpne   p1.b, p0/z, z0.b, z1.b
    b.any   L(mismatch)

    ld1b    z0.b, p0/z, [src1, off_vl]
    ld1b    z1.b, p0/z, [src2, off_vl]
    cmpne   p1.b, p0/z, z0.b, z1.b
    b.any   L(mismatch)

    ld1b    z0.b, p0/z, [src1, off_vlx2]
    ld1b    z1.b, p0/z, [src2, off_vlx2]
    cmpne   p1.b, p0/z, z0.b, z1.b
    b.any   L(mismatch)

    ld1b    z0.b, p0/z, [src1, off_vlx3]
    ld1b    z1.b, p0/z, [src2, off_vlx3]
    cmpne   p1.b, p0/z, z0.b, z1.b
    b.any   L(mismatch)

    add     src1, src1, off_vlx4
    add     src2, src2, off_vlx4
    subs    cnt, cnt, off_vlx4
    cmp     cnt, off_vlx4
    b.hs    L(loop_full)

L(tail_4xvl):
    whilelo p0.b, xzr, cnt
    b.none  L(equal)
    ld1b    z0.b, p0/z, [src1]
    ld1b    z1.b, p0/z, [src2]
    cmpne   p1.b, p0/z, z0.b, z1.b
    b.any   L(mismatch)

    whilelo p0.b, off_vl, cnt
    b.none  L(equal)
    ld1b    z0.b, p0/z, [src1, off_vl]
    ld1b    z1.b, p0/z, [src2, off_vl]
    cmpne   p1.b, p0/z, z0.b, z1.b
    b.any   L(mismatch)

    whilelo p0.b, off_vlx2, cnt
    b.none  L(equal)
    ld1b    z0.b, p0/z, [src1, off_vlx2]
    ld1b    z1.b, p0/z, [src2, off_vlx2]
    cmpne   p1.b, p0/z, z0.b, z1.b
    b.any   L(mismatch)

    whilelo p0.b, off_vlx3, cnt
    b.none  L(equal)
    ld1b    z0.b, p0/z, [src1, off_vlx3]
    ld1b    z1.b, p0/z, [src2, off_vlx3]
    cmpne   p1.b, p0/z, z0.b, z1.b
    b.any   L(mismatch)

L(equal):
    mov     result, #0
    ret

L(mismatch):
    mov     result, #1
    cmphi   p2.b, p1/z, z1.b, z0.b
    cneg    result, result, mi
    ret
END (__memcmp_kunpeng950)
