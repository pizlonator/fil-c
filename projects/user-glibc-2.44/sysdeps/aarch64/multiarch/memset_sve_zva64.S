/* Optimized memset for SVE.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.

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
 * ARMv8-a, AArch64, Advanced SIMD, SVE, unaligned accesses.
 * ZVA size is 64.
 */

.arch armv8.2-a+sve

#define dstin	x0
#define val	x1
#define valw	w1
#define count	x2
#define dst	x3
#define dstend	x4
#define zva_val	x5
#define vlen	x5
#define off	x3
#define dstend2 x5

ENTRY (__memset_sve_zva64)
	dup	v0.16B, valw
	cmp	count, 16
	b.lo	L(set_16)

	add	dstend, dstin, count
	cmp	count, 64
	b.hi	L(set_128)

	/* Set 16..64 bytes.  */
	mov	off, 48
	and	off, off, count, lsr 1
	sub	dstend2, dstend, off
	str	q0, [dstin]
	str	q0, [dstin, off]
	str	q0, [dstend2, -16]
	str	q0, [dstend, -16]
	ret

	.p2align 4
L(set_16):
	whilelo p0.b, xzr, count
	st1b	z0.b, p0, [dstin]
	ret

	.p2align 4
L(set_128):
	bic	dst, dstin, 15
	cmp	count, 128
	b.hi	L(set_long)
	stp	q0, q0, [dstin]
	stp	q0, q0, [dstin, 32]
	stp	q0, q0, [dstend, -64]
	stp	q0, q0, [dstend, -32]
	ret

	.p2align 4
L(set_long):
	cmp	count, 256
	b.lo	L(no_zva)
	tst	valw, 255
	b.ne	L(no_zva)

	str	q0, [dstin]
	str	q0, [dst, 16]
	bic	dst, dstin, 31
	stp	q0, q0, [dst, 32]
	bic	dst, dstin, 63
	sub	count, dstend, dst	/* Count is now 64 too large.  */
	sub	count, count, 128	/* Adjust count and bias for loop.  */

	sub	x8, dstend, 1		/* Write last bytes before ZVA loop.  */
	bic	x8, x8, 15
	stp	q0, q0, [x8, -48]
	str	q0, [x8, -16]
	str	q0, [dstend, -16]

	.p2align 4
L(zva64_loop):
	add	dst, dst, 64
	dc	zva, dst
	subs	count, count, 64
	b.hi	L(zva64_loop)
	ret

L(no_zva):
	str	q0, [dstin]
	sub	count, dstend, dst	/* Count is 16 too large.  */
	sub	count, count, 64 + 16	/* Adjust count and bias for loop.  */
L(no_zva_loop):
	stp	q0, q0, [dst, 16]
	stp	q0, q0, [dst, 48]
	add	dst, dst, 64
	subs	count, count, 64
	b.hi	L(no_zva_loop)
	stp	q0, q0, [dstend, -64]
	stp	q0, q0, [dstend, -32]
	ret

END (__memset_sve_zva64)
