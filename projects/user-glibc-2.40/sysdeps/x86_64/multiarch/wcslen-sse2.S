/* wcslen optimized with SSE2.
   Copyright (C) 2017-2024 Free Software Foundation, Inc.
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

#include <isa-level.h>

#if ISA_SHOULD_BUILD (1)

# include <sysdep.h>

#ifndef WCSLEN
# define WCSLEN	__wcslen_sse2
#endif


	.text
ENTRY (WCSLEN)
	cmpl	$0, (%rdi)
	jz	L(exit_tail0)
	cmpl	$0, 4(%rdi)
	jz	L(exit_tail1)
	cmpl	$0, 8(%rdi)
	jz	L(exit_tail2)
	cmpl	$0, 12(%rdi)
	jz	L(exit_tail3)
	cmpl	$0, 16(%rdi)
	jz	L(exit_tail4)
	cmpl	$0, 20(%rdi)
	jz	L(exit_tail5)
	cmpl	$0, 24(%rdi)
	jz	L(exit_tail6)
	cmpl	$0, 28(%rdi)
	jz	L(exit_tail7)

	pxor	%xmm0, %xmm0

	lea	32(%rdi), %rax
	addq	$16, %rdi
	and	$-16, %rax

	pcmpeqd	(%rax), %xmm0
	pmovmskb %xmm0, %edx
	pxor	%xmm1, %xmm1
	addq	$16, %rax
	test	%edx, %edx
	jnz	L(exit)

	pcmpeqd	(%rax), %xmm1
	pmovmskb %xmm1, %edx
	pxor	%xmm2, %xmm2
	addq	$16, %rax
	test	%edx, %edx
	jnz	L(exit)

	pcmpeqd	(%rax), %xmm2
	pmovmskb %xmm2, %edx
	pxor	%xmm3, %xmm3
	addq	$16, %rax
	test	%edx, %edx
	jnz	L(exit)

	pcmpeqd	(%rax), %xmm3
	pmovmskb %xmm3, %edx
	addq	$16, %rax
	test	%edx, %edx
	jnz	L(exit)

	pcmpeqd	(%rax), %xmm0
	pmovmskb %xmm0, %edx
	addq	$16, %rax
	test	%edx, %edx
	jnz	L(exit)

	pcmpeqd	(%rax), %xmm1
	pmovmskb %xmm1, %edx
	addq	$16, %rax
	test	%edx, %edx
	jnz	L(exit)

	pcmpeqd	(%rax), %xmm2
	pmovmskb %xmm2, %edx
	addq	$16, %rax
	test	%edx, %edx
	jnz	L(exit)

	pcmpeqd	(%rax), %xmm3
	pmovmskb %xmm3, %edx
	addq	$16, %rax
	test	%edx, %edx
	jnz	L(exit)

	pcmpeqd	(%rax), %xmm0
	pmovmskb %xmm0, %edx
	addq	$16, %rax
	test	%edx, %edx
	jnz	L(exit)

	pcmpeqd	(%rax), %xmm1
	pmovmskb %xmm1, %edx
	addq	$16, %rax
	test	%edx, %edx
	jnz	L(exit)

	pcmpeqd	(%rax), %xmm2
	pmovmskb %xmm2, %edx
	addq	$16, %rax
	test	%edx, %edx
	jnz	L(exit)

	pcmpeqd	(%rax), %xmm3
	pmovmskb %xmm3, %edx
	addq	$16, %rax
	test	%edx, %edx
	jnz	L(exit)

	and	$-0x40, %rax

	.p2align 4
L(aligned_64_loop):
	movaps	(%rax), %xmm0
	movaps	16(%rax), %xmm1
	movaps	32(%rax), %xmm2
	movaps	48(%rax), %xmm6

	pminub	%xmm1, %xmm0
	pminub	%xmm6, %xmm2
	pminub	%xmm0, %xmm2
	pcmpeqd	%xmm3, %xmm2
	pmovmskb %xmm2, %edx
	addq	$64, %rax
	test	%edx, %edx
	jz	L(aligned_64_loop)

	pcmpeqd	-64(%rax), %xmm3
	pmovmskb %xmm3, %edx
    addq	$48, %rdi
	test	%edx, %edx
	jnz	L(exit)

	pcmpeqd	%xmm1, %xmm3
	pmovmskb %xmm3, %edx
    addq	$-16, %rdi
	test	%edx, %edx
	jnz	L(exit)

	pcmpeqd	-32(%rax), %xmm3
	pmovmskb %xmm3, %edx
    addq	$-16, %rdi
	test	%edx, %edx
	jnz	L(exit)

	pcmpeqd	%xmm6, %xmm3
	pmovmskb %xmm3, %edx
    addq	$-16, %rdi
	test	%edx, %edx
	jz	L(aligned_64_loop)

	.p2align 4
L(exit):
	sub	%rdi, %rax
	shr	$2, %rax
	test	%dl, %dl
	jz	L(exit_high)

	andl	$15, %edx
	jz	L(exit_1)
	ret

	/* No align here. Naturally aligned % 16 == 1.  */
L(exit_high):
	andl	$(15 << 8), %edx
	jz	L(exit_3)
	add	$2, %rax
	ret

	.p2align 3
L(exit_1):
	add	$1, %rax
	ret

	.p2align 3
L(exit_3):
	add	$3, %rax
	ret

	.p2align 3
L(exit_tail0):
	xorl	%eax, %eax
	ret

	.p2align 3
L(exit_tail1):
	movl	$1, %eax
	ret

	.p2align 3
L(exit_tail2):
	movl	$2, %eax
	ret

	.p2align 3
L(exit_tail3):
	movl	$3, %eax
	ret

	.p2align 3
L(exit_tail4):
	movl	$4, %eax
	ret

	.p2align 3
L(exit_tail5):
	movl	$5, %eax
	ret

	.p2align 3
L(exit_tail6):
	movl	$6, %eax
	ret

	.p2align 3
L(exit_tail7):
	movl	$7, %eax
	ret

END (WCSLEN)

#endif
