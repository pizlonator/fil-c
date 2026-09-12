/* Optimized memset for Qualcomm's oyron-1 core.
   Copyright (C) 2018-2026 Free Software Foundation, Inc.
   Copyright The GNU Toolchain Authors.

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
   ARMv8-a, AArch64, unaligned accesses
 */

#define dstin	x0
#define val	x1
#define valw	w1
#define count	x2
#define dst	x3
#define dstend	x4

ENTRY (__memset_oryon1)

	bfi	valw, valw, 8, 8
	bfi	valw, valw, 16, 16
	bfi	val, val, 32, 32

	add	dstend, dstin, count

	cmp	count, 96
	b.hi	L(set_long)
	cmp	count, 16
	b.hs	L(set_medium)

	/* Set 0..15 bytes.  */
	tbz	count, 3, 1f
	str	val, [dstin]
	str	val, [dstend, -8]
	ret

	.p2align 3
1:	tbz	count, 2, 2f
	str	valw, [dstin]
	str	valw, [dstend, -4]
	ret
2:	cbz	count, 3f
	strb	valw, [dstin]
	tbz	count, 1, 3f
	strh	valw, [dstend, -2]
3:	ret

	.p2align 3
	/* Set 16..96 bytes.  */
L(set_medium):
	stp	val, val, [dstin]
	tbnz	count, 6, L(set96)
	stp	val, val, [dstend, -16]
	tbz	count, 5, 1f
	stp	val, val, [dstin, 16]
	stp	val, val, [dstend, -32]
1:	ret

	.p2align 6
	/* Set 64..96 bytes.  Write 64 bytes from the start and
	   32 bytes from the end.  */
L(set96):
	stp	val, val, [dstin, 16]
	stp	val, val, [dstin, 32]
	stp	val, val, [dstin, 48]
	stp	val, val, [dstend, -32]
	stp	val, val, [dstend, -16]
	ret

	.p2align 6
L(set_long):
	stp	val, val, [dstin]
	bic	dst, dstin, 15
	cmp	count, 256
	ccmp	valw, 0, 0, cs
	b.eq	L(try_zva)
	cmp	count, #32768
	b.hi	L(set_long_with_nontemp)
	/* Small-size or non-zero memset does not use DC ZVA. */
	sub	count, dstend, dst

	/* Adjust count and bias for loop. By subtracting extra 1 from count,
	  it is easy to use tbz instruction to check whether loop tailing
	  count is less than 33 bytes, so as to bypass 2 unnecessary stps. */
	sub	count, count, 64+16+1

1:	stp	val, val, [dst, 16]
	stp	val, val, [dst, 32]
	stp	val, val, [dst, 48]
	stp	val, val, [dst, 64]!
	subs	count, count, 64
	b.hs	1b

	tbz	count, 5, 1f	/* Remaining count is less than 33 bytes? */
	stp	val, val, [dst, 16]
	stp	val, val, [dst, 32]
1:	stp	val, val, [dstend, -32]
	stp	val, val, [dstend, -16]
	ret

L(set_long_with_nontemp):
	/* Small-size or non-zero memset does not use DC ZVA. */
	sub	count, dstend, dst

	/* Adjust count and bias for loop. By subtracting extra 1 from count,
	   it is easy to use tbz instruction to check whether loop tailing
	   count is less than 33 bytes, so as to bypass 2 unnecessary stps. */
	sub	count, count, 64+16+1

1:	stnp	val, val, [dst, 16]
	stnp	val, val, [dst, 32]
	stnp	val, val, [dst, 48]
	stnp	val, val, [dst, 64]
	add	dst, dst, #64
	subs	count, count, 64
	b.hs	1b

	tbz	count, 5, 1f	/* Remaining count is less than 33 bytes? */
	stnp	val, val, [dst, 16]
	stnp	val, val, [dst, 32]
1:	stnp	val, val, [dstend, -32]
	stnp	val, val, [dstend, -16]
	ret

L(try_zva):
	/* Write the first and last 64 byte aligned block using stp rather
	   than using DC ZVA as it is faster. */
	.p2align 6
L(zva_64):
	stp	val, val, [dst, 16]
	stp	val, val, [dst, 32]
	stp	val, val, [dst, 48]
	bic	dst, dst, 63
	stp	val, val, [dst, 64]
	stp	val, val, [dst, 64+16]
	stp	val, val, [dst, 96]
	stp	val, val, [dst, 96+16]
	sub	count, dstend, dst	/* Count is now 128 too large.	*/
	sub	count, count, 128+64+64	/* Adjust count and bias for loop.  */
	add	dst, dst, 128
1:	dc	zva, dst
	add	dst, dst, 64
	subs	count, count, 64
	b.hi	1b
	stp	val, val, [dst, 0]
	stp	val, val, [dst, 16]
	stp	val, val, [dst, 32]
	stp	val, val, [dst, 48]

	stp	val, val, [dstend, -64]
	stp	val, val, [dstend, -64+16]
	stp	val, val, [dstend, -32]
	stp	val, val, [dstend, -16]
	ret

END (__memset_oryon1)
