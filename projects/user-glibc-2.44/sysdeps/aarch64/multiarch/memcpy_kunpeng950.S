/* Optimized memcpy for Huawei Kunpeng 950 processor.
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

#define dstin	x0
#define src	x1
#define count	x2
#define dst	x3
#define srcend	x4
#define dstend	x5
#define vlen	x6
#define tmp1	x7

#define A_q	q0
#define B_q	q1
#define C_q	q2
#define D_q	q3
#define E_q	q4
#define F_q	q5
#define G_q	q6
#define H_q	q7

.arch armv8.2-a+sve

ENTRY (__memcpy_kunpeng950)
	cmp	count, 128
	b.hi	L(128_more)
	cntb	vlen
	cmp	count, vlen, lsl 1
	b.hi	L(copy_2vl_more)
L(copy_0_2vl):
	whilelo p0.b, xzr, count
	whilelo p1.b, vlen, count
	ld1b	z0.b, p0/z, [src]
	ld1b	z1.b, p1/z, [src, 1, mul vl]
	st1b	z0.b, p0, [dstin]
	st1b	z1.b, p1, [dstin, 1, mul vl]
	ret

	.p2align 4
L(copy_2vl_more):
	add	srcend, src, count
	add	dstend, dstin, count
	ldp	A_q, B_q, [src]
	ldp	C_q, D_q, [srcend, -32]
	cmp	count, 64
	b.ls	L(copy0_64)
	ldp	E_q, F_q, [src, 32]
	ldp	G_q, H_q, [srcend, -64]
	stp	A_q, B_q, [dstin]
	stp	C_q, D_q, [dstend, -32]
	stp	E_q, F_q, [dstin, 32]
	stp	G_q, H_q, [dstend, -64]
	ret
L(copy0_64):
	stp	A_q, B_q, [dstin]
	stp	C_q, D_q, [dstend, -32]
	ret

	.p2align 4
L(128_more):
	ldp	E_q, F_q, [src]
	add	srcend, src, count
	add	dstend, dstin, count

	and	tmp1, dstin, 31
	bic	dst, dstin, 31
	sub	src, src, tmp1
	add	count, count, tmp1

	ldp	A_q, B_q, [src, 32]
	ldp	C_q, D_q, [src, 64]!
	stp	E_q, F_q, [dstin]
	subs	count, count, 64+64+32
	b.ls	L(copy64_from_end)

L(loop64):
	stp	A_q, B_q, [dst, 32]
	ldp	A_q, B_q, [src, 32]
	stp	C_q, D_q, [dst, 64]!
	ldp	C_q, D_q, [src, 64]!
	subs	count, count, 64
	b.hi	L(loop64)
L(copy64_from_end):
	ldp	E_q, F_q, [srcend, -64]
	stp	A_q, B_q, [dst,32]
	ldp	G_q, H_q, [srcend, -32]
	stp	C_q, D_q, [dst, 64]
	stp	E_q, F_q, [dstend, -64]
	stp	G_q, H_q, [dstend, -32]
	ret
END (__memcpy_kunpeng950)
