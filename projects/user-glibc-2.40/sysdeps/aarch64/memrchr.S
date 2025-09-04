/* memrchr - find the last occurrence of a byte in a memory block

   Copyright (C) 2015-2024 Free Software Foundation, Inc.

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
 * ARMv8-a, AArch64, Advanced SIMD.
 * MTE compatible.
 */

#define srcin		x0
#define chrin		w1
#define cntin		x2
#define result		x0

#define src		x3
#define cntrem		x4
#define synd		x5
#define shift		x6
#define	tmp		x7
#define end		x8
#define endm1		x9

#define vrepchr		v0
#define qdata		q1
#define vdata		v1
#define vhas_chr	v2
#define vend		v3
#define dend		d3

/*
   Core algorithm:
   For each 16-byte chunk we calculate a 64-bit nibble mask value with four bits
   per byte. We take 4 bits of every comparison byte with shift right and narrow
   by 4 instruction. Since the bits in the nibble mask reflect the order in
   which things occur in the original string, counting leading zeros identifies
   exactly which byte matched.  */

ENTRY (__memrchr)
	PTR_ARG (0)
	SIZE_ARG (2)
	add	end, srcin, cntin
	sub	endm1, end, 1
	bic	src, endm1, 15
	cbz	cntin, L(nomatch)
	ld1	{vdata.16b}, [src]
	dup	vrepchr.16b, chrin
	cmeq	vhas_chr.16b, vdata.16b, vrepchr.16b
	neg	shift, end, lsl 2
	shrn	vend.8b, vhas_chr.8h, 4		/* 128->64 */
	fmov	synd, dend
	lsl	synd, synd, shift
	cbz	synd, L(start_loop)

	clz	synd, synd
	sub	result, endm1, synd, lsr 2
	cmp	cntin, synd, lsr 2
	csel	result, result, xzr, hi
	ret

	nop
L(start_loop):
	subs	cntrem, src, srcin
	b.ls	L(nomatch)

	/* Make sure that it won't overread by a 16-byte chunk */
	sub	cntrem, cntrem, 1
	tbz	cntrem, 4, L(loop32_2)
	add	src, src, 16

	.p2align 5
L(loop32):
	ldr	qdata, [src, -32]!
	cmeq	vhas_chr.16b, vdata.16b, vrepchr.16b
	umaxp	vend.16b, vhas_chr.16b, vhas_chr.16b		/* 128->64 */
	fmov	synd, dend
	cbnz	synd, L(end)

L(loop32_2):
	ldr	qdata, [src, -16]
	subs	cntrem, cntrem, 32
	cmeq	vhas_chr.16b, vdata.16b, vrepchr.16b
	b.lo	L(end_2)
	umaxp	vend.16b, vhas_chr.16b, vhas_chr.16b		/* 128->64 */
	fmov	synd, dend
	cbz	synd, L(loop32)
L(end_2):
	sub	src, src, 16
L(end):
	shrn	vend.8b, vhas_chr.8h, 4		/* 128->64 */
	fmov	synd, dend

	add	tmp, src, 15
#ifdef __AARCH64EB__
	rbit	synd, synd
#endif
	clz	synd, synd
	sub	tmp, tmp, synd, lsr 2
	cmp	tmp, srcin
	csel	result, tmp, xzr, hs
	ret

L(nomatch):
	mov	result, 0
	ret

END (__memrchr)
libc_hidden_def (__memrchr)
weak_alias (__memrchr, memrchr)
libc_hidden_builtin_def (memrchr)
