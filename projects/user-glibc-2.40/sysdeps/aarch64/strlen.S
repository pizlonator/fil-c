/* Copyright (C) 2012-2024 Free Software Foundation, Inc.

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

#ifndef STRLEN
# define STRLEN __strlen
#endif

#define srcin		x0
#define result		x0

#define src		x1
#define	synd		x2
#define tmp		x3
#define shift		x4

#define data		q0
#define vdata		v0
#define vhas_nul	v1
#define vend		v2
#define dend		d2

/* Core algorithm:
   Process the string in 16-byte aligned chunks. Compute a 64-bit mask with
   four bits per byte using the shrn instruction. A count trailing zeros then
   identifies the first zero byte.  */

ENTRY (STRLEN)
	PTR_ARG (0)
	bic	src, srcin, 15
	ld1	{vdata.16b}, [src]
	cmeq	vhas_nul.16b, vdata.16b, 0
	lsl	shift, srcin, 2
	shrn	vend.8b, vhas_nul.8h, 4		/* 128->64 */
	fmov	synd, dend
	lsr	synd, synd, shift
	cbz	synd, L(loop)

	rbit	synd, synd
	clz	result, synd
	lsr	result, result, 2
	ret

	.p2align 5
L(loop):
	ldr	data, [src, 16]
	cmeq	vhas_nul.16b, vdata.16b, 0
	umaxp	vend.16b, vhas_nul.16b, vhas_nul.16b
	fmov	synd, dend
	cbnz	synd, L(loop_end)
	ldr	data, [src, 32]!
	cmeq	vhas_nul.16b, vdata.16b, 0
	umaxp	vend.16b, vhas_nul.16b, vhas_nul.16b
	fmov	synd, dend
	cbz	synd, L(loop)
	sub	src, src, 16
L(loop_end):
	shrn	vend.8b, vhas_nul.8h, 4		/* 128->64 */
	sub	result, src, srcin
	fmov	synd, dend
#ifndef __AARCH64EB__
	rbit	synd, synd
#endif
	add	result, result, 16
	clz	tmp, synd
	add	result, result, tmp, lsr 2
	ret

END (STRLEN)
weak_alias (STRLEN, strlen)
libc_hidden_builtin_def (strlen)
