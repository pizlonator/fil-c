/* Optimized memset for AmpereComputing emag processor.
   Copyright (C) 2018-2024 Free Software Foundation, Inc.

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
#include "memset-reg.h"

/* Assumptions:
 *
 * ARMv8-a, AArch64, unaligned accesses
 *
 */

ENTRY (__memset_emag)

	PTR_ARG (0)
	SIZE_ARG (2)

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

	.p2align 4
	/* Set 64..96 bytes.  Write 64 bytes from the start and
	   32 bytes from the end.  */
L(set96):
	stp	val, val, [dstin, 16]
	stp	val, val, [dstin, 32]
	stp	val, val, [dstin, 48]
	stp	val, val, [dstend, -32]
	stp	val, val, [dstend, -16]
	ret

	.p2align 4
L(set_long):
	stp	val, val, [dstin]
	bic	dst, dstin, 15
	/* Small-size or non-zero memset does not use DC ZVA. */
	sub	count, dstend, dst

	/*
	 * Adjust count and bias for loop. By subtracting extra 1 from count,
	 * it is easy to use tbz instruction to check whether loop tailing
	 * count is less than 33 bytes, so as to bypass 2 unnecessary stps.
	 */
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

END (__memset_emag)
