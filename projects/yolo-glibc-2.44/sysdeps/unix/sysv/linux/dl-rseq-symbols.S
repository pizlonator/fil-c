/* Define symbols used by rseq.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <sysdep.h>

#if __WORDSIZE == 64
#define RSEQ_OFFSET_SIZE	8
#else
#define RSEQ_OFFSET_SIZE	4
#endif

/* Some targets define a macro to denote the zero register.  */
#undef zero

/* Define 3 symbols: '__rseq_size' is public const and then '_rseq_size' and
   '__GI___rseq_size' (both aliases of '__rseq_size') are hidden, '_rseq_size'
   is writable for internal use by the dynamic linker which will initialize
   the value the symbols point to before copy relocations take place.  */

	.globl	__rseq_size
	.type	__rseq_size, %object
	.size	__rseq_size, 4
	.hidden __GI___rseq_size
	.globl	__GI___rseq_size
	.type	__GI___rseq_size, %object
	.size	__GI___rseq_size, 4
	.hidden _rseq_size
	.globl	_rseq_size
	.type	_rseq_size, %object
	.size	_rseq_size, 4
	.section .data.rel.ro
	.balign 4
__rseq_size:
__GI___rseq_size:
_rseq_size:
	.zero	4

/* Define 3 symbols: '__rseq_offset' is public const and then '_rseq_offset'
   and '__GI___rseq_offset' (both aliases of '__rseq_offset') are hidden,
   '_rseq_offset' is writable for internal use by the dynamic linker which will
   initialize the value the symbols point to before copy relocations take
   place.  */

	.globl	__rseq_offset
	.type	__rseq_offset, %object
	.size	__rseq_offset, RSEQ_OFFSET_SIZE
	.hidden __GI___rseq_offset
	.globl	__GI___rseq_offset
	.type	__GI___rseq_offset, %object
	.size	__GI___rseq_offset, RSEQ_OFFSET_SIZE
	.hidden _rseq_offset
	.globl	_rseq_offset
	.type	_rseq_offset, %object
	.size	_rseq_offset, RSEQ_OFFSET_SIZE
	.section .data.rel.ro
	.balign RSEQ_OFFSET_SIZE
__rseq_offset:
__GI___rseq_offset:
_rseq_offset:
	.zero	RSEQ_OFFSET_SIZE
