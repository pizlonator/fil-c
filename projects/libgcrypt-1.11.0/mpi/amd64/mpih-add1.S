/* AMD64 (x86_64) add_n -- Add two limb vectors of the same length > 0 and store
 *		   sum in a third limb vector.
 *
 *      Copyright (C) 1992, 1994, 1995, 1998, 
 *                    2001, 2002, 2006 Free Software Foundation, Inc.
 *      Copyright (C) 2023 Jussi Kivilinna <jussi.kivilinna@iki.fi>
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgcrypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <https://www.gnu.org/licenses/>.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Note: This code is heavily based on the GNU MP Library.
 *	 Actually it's the same code with only minor changes in the
 *	 way the data is stored; this is to support the abstraction
 *	 of an optional secure memory allocation which may be used
 *	 to avoid revealing of sensitive data due to paging etc.
 */


#include "sysdep.h"
#include "asm-syntax.h"


/*******************
 *  mpi_limb_t
 *  _gcry_mpih_add_n( mpi_ptr_t res_ptr,	rdi
 *		   mpi_ptr_t s1_ptr,		rsi
 *		   mpi_ptr_t s2_ptr,		rdx
 *		   mpi_size_t size)		rcx
 */
	TEXT
	ALIGN(4)
	.globl C_SYMBOL_NAME(_gcry_mpih_add_n)
C_SYMBOL_NAME(_gcry_mpih_add_n:)
	FUNC_ENTRY()
	movl	%ecx, %r9d
	andl	$3, %r9d
	je	.Lprehandle0
	cmpl	$2, %r9d
	jb	.Lprehandle1
	je	.Lprehandle2

#define FIRST_ADD() \
	movq	(%rsi), %rax; \
	addq	(%rdx), %rax; \
	movq	%rax, (%rdi)

#define NEXT_ADD(offset) \
	movq	offset(%rsi), %rax; \
	adcq	offset(%rdx), %rax; \
	movq	%rax, offset(%rdi)

.Lprehandle3:
	leaq	-2(%rcx), %rcx
	FIRST_ADD();
	NEXT_ADD(8);
	NEXT_ADD(16);
	decq	%rcx
	je	.Lend
	leaq	24(%rsi), %rsi
	leaq	24(%rdx), %rdx
	leaq	24(%rdi), %rdi
	jmp	.Loop

	ALIGN(3)
.Lprehandle2:
	leaq	-1(%rcx), %rcx
	FIRST_ADD();
	NEXT_ADD(8);
	decq	%rcx
	je	.Lend
	leaq	16(%rsi), %rsi
	leaq	16(%rdx), %rdx
	leaq	16(%rdi), %rdi
	jmp	.Loop

	ALIGN(3)
.Lprehandle1:
	FIRST_ADD();
	decq	%rcx
	je	.Lend
	leaq	8(%rsi), %rsi
	leaq	8(%rdx), %rdx
	leaq	8(%rdi), %rdi
	jmp	.Loop

	ALIGN(3)
.Lprehandle0:
	clc				/* clear cy */

	ALIGN(4)			/* minimal alignment for claimed speed */
.Loop:	leaq	-3(%rcx), %rcx
	NEXT_ADD(0);
	NEXT_ADD(8);
	NEXT_ADD(16);
	NEXT_ADD(24);
	leaq	32(%rsi), %rsi
	leaq	32(%rdx), %rdx
	leaq	32(%rdi), %rdi
	decq	%rcx
	jne	.Loop

	ALIGN(2)
.Lend:
	movl	$0, %eax		/* zero %rax */
	adcl	%eax, %eax
	FUNC_EXIT()
