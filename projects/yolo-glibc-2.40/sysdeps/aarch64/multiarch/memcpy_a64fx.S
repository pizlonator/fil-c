/* Optimized memcpy for Fujitsu A64FX processor.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.

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

#undef BTI_C
#define BTI_C

/* Assumptions:
 *
 * ARMv8.2-a, AArch64, unaligned accesses, sve
 *
 */

#define dstin	x0
#define src	x1
#define n	x2
#define dst	x3
#define dstend	x4
#define srcend	x5
#define tmp	x6
#define vlen	x7
#define vlen8	x8

#if HAVE_AARCH64_SVE_ASM

	.arch armv8.2-a+sve

	.macro ld1b_unroll8
	ld1b	z0.b, p0/z, [src, 0, mul vl]
	ld1b	z1.b, p0/z, [src, 1, mul vl]
	ld1b	z2.b, p0/z, [src, 2, mul vl]
	ld1b	z3.b, p0/z, [src, 3, mul vl]
	ld1b	z4.b, p0/z, [src, 4, mul vl]
	ld1b	z5.b, p0/z, [src, 5, mul vl]
	ld1b	z6.b, p0/z, [src, 6, mul vl]
	ld1b	z7.b, p0/z, [src, 7, mul vl]
	.endm

	.macro stld1b_unroll4a
	st1b	z0.b, p0,   [dst, 0, mul vl]
	st1b	z1.b, p0,   [dst, 1, mul vl]
	ld1b	z0.b, p0/z, [src, 0, mul vl]
	ld1b	z1.b, p0/z, [src, 1, mul vl]
	st1b	z2.b, p0,   [dst, 2, mul vl]
	st1b	z3.b, p0,   [dst, 3, mul vl]
	ld1b	z2.b, p0/z, [src, 2, mul vl]
	ld1b	z3.b, p0/z, [src, 3, mul vl]
	.endm

	.macro stld1b_unroll4b
	st1b	z4.b, p0,   [dst, 4, mul vl]
	st1b	z5.b, p0,   [dst, 5, mul vl]
	ld1b	z4.b, p0/z, [src, 4, mul vl]
	ld1b	z5.b, p0/z, [src, 5, mul vl]
	st1b	z6.b, p0,   [dst, 6, mul vl]
	st1b	z7.b, p0,   [dst, 7, mul vl]
	ld1b	z6.b, p0/z, [src, 6, mul vl]
	ld1b	z7.b, p0/z, [src, 7, mul vl]
	.endm

	.macro stld1b_unroll8
	stld1b_unroll4a
	stld1b_unroll4b
	.endm

	.macro st1b_unroll8
	st1b	z0.b, p0, [dst, 0, mul vl]
	st1b	z1.b, p0, [dst, 1, mul vl]
	st1b	z2.b, p0, [dst, 2, mul vl]
	st1b	z3.b, p0, [dst, 3, mul vl]
	st1b	z4.b, p0, [dst, 4, mul vl]
	st1b	z5.b, p0, [dst, 5, mul vl]
	st1b	z6.b, p0, [dst, 6, mul vl]
	st1b	z7.b, p0, [dst, 7, mul vl]
	.endm

#undef BTI_C
#define BTI_C

ENTRY (__memcpy_a64fx)

	PTR_ARG (0)
	PTR_ARG (1)
	SIZE_ARG (2)

	cntb	vlen
	cmp	n, vlen, lsl 1
	b.hi	L(copy_small)
	whilelo	p1.b, vlen, n
	whilelo	p0.b, xzr, n
	ld1b	z0.b, p0/z, [src, 0, mul vl]
	ld1b	z1.b, p1/z, [src, 1, mul vl]
	st1b	z0.b, p0, [dstin, 0, mul vl]
	st1b	z1.b, p1, [dstin, 1, mul vl]
	ret

	.p2align 4

L(copy_small):
	cmp	n, vlen, lsl 3
	b.hi	L(copy_large)
	add	dstend, dstin, n
	add	srcend, src, n
	cmp	n, vlen, lsl 2
	b.hi	1f

	/* Copy 2-4 vectors.  */
	ptrue	p0.b
	ld1b	z0.b, p0/z, [src, 0, mul vl]
	ld1b	z1.b, p0/z, [src, 1, mul vl]
	ld1b	z2.b, p0/z, [srcend, -2, mul vl]
	ld1b	z3.b, p0/z, [srcend, -1, mul vl]
	st1b	z0.b, p0, [dstin, 0, mul vl]
	st1b	z1.b, p0, [dstin, 1, mul vl]
	st1b	z2.b, p0, [dstend, -2, mul vl]
	st1b	z3.b, p0, [dstend, -1, mul vl]
	ret

	.p2align 4
	/* Copy 4-8 vectors.  */
1:	ptrue	p0.b
	ld1b	z0.b, p0/z, [src, 0, mul vl]
	ld1b	z1.b, p0/z, [src, 1, mul vl]
	ld1b	z2.b, p0/z, [src, 2, mul vl]
	ld1b	z3.b, p0/z, [src, 3, mul vl]
	ld1b	z4.b, p0/z, [srcend, -4, mul vl]
	ld1b	z5.b, p0/z, [srcend, -3, mul vl]
	ld1b	z6.b, p0/z, [srcend, -2, mul vl]
	ld1b	z7.b, p0/z, [srcend, -1, mul vl]
	st1b	z0.b, p0, [dstin, 0, mul vl]
	st1b	z1.b, p0, [dstin, 1, mul vl]
	st1b	z2.b, p0, [dstin, 2, mul vl]
	st1b	z3.b, p0, [dstin, 3, mul vl]
	st1b	z4.b, p0, [dstend, -4, mul vl]
	st1b	z5.b, p0, [dstend, -3, mul vl]
	st1b	z6.b, p0, [dstend, -2, mul vl]
	st1b	z7.b, p0, [dstend, -1, mul vl]
	ret

	.p2align 4
	/* At least 8 vectors - always align to vector length for
	   higher and consistent write performance.  */
L(copy_large):
	sub	tmp, vlen, 1
	and	tmp, dstin, tmp
	sub	tmp, vlen, tmp
	whilelo	p1.b, xzr, tmp
	ld1b	z1.b, p1/z, [src]
	st1b	z1.b, p1, [dstin]
	add	dst, dstin, tmp
	add	src, src, tmp
	sub	n, n, tmp
	ptrue	p0.b

	lsl	vlen8, vlen, 3
	subs	n, n, vlen8
	b.ls	3f
	ld1b_unroll8
	add	src, src, vlen8
	subs	n, n, vlen8
	b.ls	2f

	.p2align 4
	/* 8x unrolled and software pipelined loop.  */
1:	stld1b_unroll8
	add	dst, dst, vlen8
	add	src, src, vlen8
	subs	n, n, vlen8
	b.hi	1b
2:	st1b_unroll8
	add	dst, dst, vlen8
3:	add	n, n, vlen8

	/* Move last 0-8 vectors.  */
L(last_bytes):
	cmp	n, vlen, lsl 1
	b.hi	1f
	whilelo	p0.b, xzr, n
	whilelo	p1.b, vlen, n
	ld1b	z0.b, p0/z, [src, 0, mul vl]
	ld1b	z1.b, p1/z, [src, 1, mul vl]
	st1b	z0.b, p0, [dst, 0, mul vl]
	st1b	z1.b, p1, [dst, 1, mul vl]
	ret

	.p2align 4

1:	add	srcend, src, n
	add	dstend, dst, n
	ld1b	z0.b, p0/z, [src, 0, mul vl]
	ld1b	z1.b, p0/z, [src, 1, mul vl]
	ld1b	z2.b, p0/z, [srcend, -2, mul vl]
	ld1b	z3.b, p0/z, [srcend, -1, mul vl]
	cmp	n, vlen, lsl 2
	b.hi	1f

	st1b	z0.b, p0, [dst, 0, mul vl]
	st1b	z1.b, p0, [dst, 1, mul vl]
	st1b	z2.b, p0, [dstend, -2, mul vl]
	st1b	z3.b, p0, [dstend, -1, mul vl]
	ret

1:	ld1b	z4.b, p0/z, [src, 2, mul vl]
	ld1b	z5.b, p0/z, [src, 3, mul vl]
	ld1b	z6.b, p0/z, [srcend, -4, mul vl]
	ld1b	z7.b, p0/z, [srcend, -3, mul vl]
	st1b	z0.b, p0, [dst, 0, mul vl]
	st1b	z1.b, p0, [dst, 1, mul vl]
	st1b	z4.b, p0, [dst, 2, mul vl]
	st1b	z5.b, p0, [dst, 3, mul vl]
	st1b	z6.b, p0, [dstend, -4, mul vl]
	st1b	z7.b, p0, [dstend, -3, mul vl]
	st1b	z2.b, p0, [dstend, -2, mul vl]
	st1b	z3.b, p0, [dstend, -1, mul vl]
	ret

END (__memcpy_a64fx)


ENTRY_ALIGN (__memmove_a64fx, 4)

	PTR_ARG (0)
	PTR_ARG (1)
	SIZE_ARG (2)

	/* Fast case for up to 2 vectors.  */
	cntb	vlen
	cmp	n, vlen, lsl 1
	b.hi	1f
	whilelo	p0.b, xzr, n
	whilelo	p1.b, vlen, n
	ld1b	z0.b, p0/z, [src, 0, mul vl]
	ld1b	z1.b, p1/z, [src, 1, mul vl]
	st1b	z0.b, p0, [dstin, 0, mul vl]
	st1b	z1.b, p1, [dstin, 1, mul vl]
L(full_overlap):
	ret

	.p2align 4
	/* Check for overlapping moves. Return if there is a full overlap.
	   Small moves up to 8 vectors use the overlap-safe copy_small code.
	   Non-overlapping or overlapping moves with dst < src use memcpy.
	   Overlapping moves with dst > src use a backward copy loop.  */
1:	sub	tmp, dstin, src
	ands	tmp, tmp, 0xffffffffffffff	/* Clear special tag bits.  */
	b.eq	L(full_overlap)
	cmp	n, vlen, lsl 3
	b.ls	L(copy_small)
	cmp	tmp, n
	b.hs	L(copy_large)

	/* Align to vector length.  */
	add	dst, dstin, n
	sub	tmp, vlen, 1
	ands	tmp, dst, tmp
	csel	tmp, tmp, vlen, ne
	whilelo	p1.b, xzr, tmp
	sub	n, n, tmp
	ld1b	z1.b, p1/z, [src, n]
	st1b	z1.b, p1, [dstin, n]
	add	src, src, n
	add	dst, dstin, n

	ptrue	p0.b
	lsl	vlen8, vlen, 3
	subs	n, n, vlen8
	b.ls	3f
	sub	src, src, vlen8
	ld1b_unroll8
	subs	n, n, vlen8
	b.ls	2f

	.p2align 4
	/* 8x unrolled and software pipelined backward copy loop.  */
1:	sub	src, src, vlen8
	sub	dst, dst, vlen8
	stld1b_unroll8
	subs	n, n, vlen8
	b.hi	1b
2:	sub	dst, dst, vlen8
	st1b_unroll8
3:	add	n, n, vlen8

	/* Adjust src/dst for last 0-8 vectors.  */
	sub	src, src, n
	mov	dst, dstin
	b	L(last_bytes)

END (__memmove_a64fx)
#endif /* HAVE_AARCH64_SVE_ASM */
