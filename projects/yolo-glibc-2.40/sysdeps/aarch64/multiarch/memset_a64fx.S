/* Optimized memset for Fujitsu A64FX processor.
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
#include <sysdeps/aarch64/memset-reg.h>

/* Assumptions:
 *
 * ARMv8.2-a, AArch64, unaligned accesses, sve
 *
 */

#define L1_SIZE		(64*1024)	// L1 64KB
#define L2_SIZE         (8*1024*1024)	// L2 8MB
#define CACHE_LINE_SIZE	256
#define PF_DIST_L1	(CACHE_LINE_SIZE * 16)	// Prefetch distance L1
#define vector_length	x9

#if HAVE_AARCH64_SVE_ASM

	.arch armv8.2-a+sve

	.macro st1b_unroll first=0, last=7
	st1b	z0.b, p0, [dst, \first, mul vl]
	.if \last-\first
	st1b_unroll "(\first+1)", \last
	.endif
	.endm


#undef BTI_C
#define BTI_C

ENTRY (__memset_a64fx)
	PTR_ARG (0)
	SIZE_ARG (2)

	cntb	vector_length
	dup	z0.b, valw
	whilelo	p0.b, vector_length, count
	b.last	1f
	whilelo	p1.b, xzr, count
	st1b	z0.b, p1, [dstin, 0, mul vl]
	st1b	z0.b, p0, [dstin, 1, mul vl]
	ret

	// count >= vector_length * 2
1:	cmp	count, vector_length, lsl 2
	add	dstend, dstin, count
	b.hi	1f
	st1b	z0.b, p0, [dstin, 0, mul vl]
	st1b	z0.b, p0, [dstin, 1, mul vl]
	st1b	z0.b, p0, [dstend, -2, mul vl]
	st1b	z0.b, p0, [dstend, -1, mul vl]
	ret

	// count > vector_length * 4
1:	lsl	tmp1, vector_length, 3
	cmp	count, tmp1
	b.hi	L(vl_agnostic)
	st1b	z0.b, p0, [dstin, 0, mul vl]
	st1b	z0.b, p0, [dstin, 1, mul vl]
	st1b	z0.b, p0, [dstin, 2, mul vl]
	st1b	z0.b, p0, [dstin, 3, mul vl]
	st1b	z0.b, p0, [dstend, -4, mul vl]
	st1b	z0.b, p0, [dstend, -3, mul vl]
	st1b	z0.b, p0, [dstend, -2, mul vl]
	st1b	z0.b, p0, [dstend, -1, mul vl]
	ret

	.p2align 4
L(vl_agnostic): // VL Agnostic
	mov	dst, dstin
	cmp	count, L1_SIZE
	b.hi	L(L1_prefetch)

	// count >= 8 * vector_length
L(unroll8):
	sub	count, count, tmp1
	.p2align 4
	// The 2 instructions at the beginning of the following loop,
	// cmp and branch, are a workaround so as not to degrade at
	// the peak performance 16KB.
	// It is found heuristically and the branch condition, b.ne,
	// is chosen intentionally never to jump.
1:	cmp	xzr, xzr
	b.ne	1b
	st1b_unroll 0, 7
	add	dst, dst, tmp1
	subs	count, count, tmp1
	b.hi	1b
	add	count, count, tmp1

L(last):
	cmp	count, vector_length, lsl 1
	b.ls	2f
	add	tmp2, vector_length, vector_length, lsl 2
	cmp	count, tmp2
	b.ls	5f
	st1b	z0.b, p0, [dstend, -8, mul vl]
	st1b	z0.b, p0, [dstend, -7, mul vl]
	st1b	z0.b, p0, [dstend, -6, mul vl]
5:	st1b	z0.b, p0, [dstend, -5, mul vl]
	st1b	z0.b, p0, [dstend, -4, mul vl]
	st1b	z0.b, p0, [dstend, -3, mul vl]
2:	st1b	z0.b, p0, [dstend, -2, mul vl]
	st1b	z0.b, p0, [dstend, -1, mul vl]
	ret

	// count >= L1_SIZE
	.p2align 3
L(L1_prefetch):
	cmp	count, L2_SIZE
	b.hs	L(L2)
	cmp	vector_length, 64
	b.ne	L(unroll8)
1:	st1b_unroll 0, 3
	prfm	pstl1keep, [dst, PF_DIST_L1]
	st1b_unroll 4, 7
	prfm	pstl1keep, [dst, PF_DIST_L1 + CACHE_LINE_SIZE]
	add	dst, dst, CACHE_LINE_SIZE * 2
	sub	count, count, CACHE_LINE_SIZE * 2
	cmp	count, PF_DIST_L1
	b.hs	1b
	b	L(unroll8)

	// count >= L2_SIZE
	.p2align 3
L(L2):
	tst	valw, 255
	b.ne	L(unroll8)
        // align dst to CACHE_LINE_SIZE byte boundary
	and	tmp2, dst, CACHE_LINE_SIZE - 1
	st1b	z0.b, p0, [dst, 0, mul vl]
	st1b	z0.b, p0, [dst, 1, mul vl]
	st1b	z0.b, p0, [dst, 2, mul vl]
	st1b	z0.b, p0, [dst, 3, mul vl]
	sub	dst, dst, tmp2
	add	count, count, tmp2

	// clear cachelines using DC ZVA
	sub	count, count, CACHE_LINE_SIZE * 2
	.p2align 4
1:	add	dst, dst, CACHE_LINE_SIZE
	dc	zva, dst
	subs	count, count, CACHE_LINE_SIZE
	b.hi	1b
	add	count, count, CACHE_LINE_SIZE
	b	L(last)

END (__memset_a64fx)

#endif /* HAVE_AARCH64_SVE_ASM */
