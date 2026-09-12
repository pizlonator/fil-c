/* Copyright (C) 2025-2026 Free Software Foundation, Inc.

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

#ifdef USE_AS_STRCSPN
# define STRSPN strcspn
# define SBT orr	/* SBT -- `set bit' */
#else
# define STRSPN strspn
# define SBT bic
#endif

#ifdef __AARCH64EB__
# define LS_FW lsl
# define LS_BK lsr
#else
# define LS_FW lsr
# define LS_BK lsl
#endif

#define og_s	x0
#define set	x1	/* ACCEPT for strspn, REJECT for strcspn */

#define byte_i	x3
#define bits_i	x4
#define one	x6

#define syndrome	x5
#define s		x6

#define vbyte_i	v1.16b
#define	vbits_i	v2.16b
#define table	v4.16b-v5.16b
#define table_a	v4
#define table_b	v5
#define sevens	v7.16b

ENTRY(STRSPN)
	ldrb	w2, [set]
	cbz	w2, L(early)
#ifdef USE_AS_STRCSPN
	ldrb	w3, [set, 1]
	cbz	w3, L(early)
#endif

	/* Table has ones for bytes to reject and zeros for bytes to accept */
	mov	one, 1
#ifdef USE_AS_STRCSPN
	stp	one, xzr, [sp, -32]!
	.cfi_def_cfa_offset 32
	stp	xzr, xzr, [sp, 16]
#else
	mvni	v0.4s, 0
	stp	q0, q0, [sp, -32]!
	.cfi_def_cfa_offset 32
#endif

	.p2align 4
L(fill_table):
	lsr	byte_i, x2, 6	/* x2 / 64 */
	lsl	bits_i, one, x2	/* x2 % 64 implicitly */
	ldrb	w2, [set, 1]!
	ldr	x5, [sp, byte_i, lsl 3]
	SBT	x5, x5, bits_i
	str	x5, [sp, byte_i, lsl 3]
	cbnz	w2, L(fill_table)

	ld1	{table_a.2d-table_b.2d}, [sp], 32
	.cfi_def_cfa_offset 0
	ubfiz	syndrome, og_s, 2, 4	/* Bottom 4 bits, times 4 to count nibbles */
	and	s, og_s, -16		/* Round S down to 16-byte boundary */
	movi	sevens, 7
	/* Bias the syndrome to mask off these nibbles */
	mov	x8, -1
	LS_BK	syndrome, x8, syndrome
	mvn	syndrome, syndrome

L(loop):
	ldr	q0, [s], 16
	ushr	vbyte_i, v0.16b, 3
	bic	vbits_i, sevens, v0.16b
	tbl	v0.16b, {table}, vbyte_i
	/* Bring the relevant bit to the MSB of each byte */
	sshl	v0.16b, v0.16b, vbits_i
	/* Set every bit of each byte to its MSB */
	cmlt	v0.16b, v0.16b, 0
	/* Bytes->nibbles */
	shrn	v0.8b, v0.8h, 4
	fmov	x2, d0
	bic	syndrome, x2, syndrome
	cbz	syndrome, L(loop)

#ifndef __AARCH64EB__
	rbit	syndrome, syndrome
#endif
	sub	s, s, 16
	clz	syndrome, syndrome
	sub	x0, s, og_s
	add	x0, x0, syndrome, lsr 2
	ret

	.balign 8 /* For strspn, which has only 2 instructions here */
L(early):
#ifdef USE_AS_STRCSPN
	/* strlen(set) < 2: call strchrnul(s, *set) and get its offset from S */
	stp	fp, lr, [sp, -32]!
	.cfi_def_cfa_offset 32
	.cfi_offset fp, -32
	.cfi_offset lr, -24
	str	x19, [sp, 16]
	.cfi_offset 19, -16
	mov	w1, w2
	mov	fp, sp
	mov	x19, x0
	bl	__strchrnul
	sub	x0, x0, x19
	ldr	x19, [sp, 16]
	ldp	fp, lr, [sp], 32
	.cfi_restore lr
	.cfi_restore fp
	.cfi_restore 19
	.cfi_def_cfa_offset 0
#else
	mov	w0, 0
#endif
	ret
END(STRSPN)

#undef set
libc_hidden_def(STRSPN)
