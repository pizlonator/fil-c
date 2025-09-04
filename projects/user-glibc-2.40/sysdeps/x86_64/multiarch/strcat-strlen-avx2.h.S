/* strlen used for beginning of str{n}cat using AVX2.
   Copyright (C) 2011-2024 Free Software Foundation, Inc.
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


/* NOTE: This file is meant to be included by strcat-avx2 or
   strncat-avx2 and does not standalone.  Before including %rdi
   must be saved in %rax.  */


/* Simple strlen implementation that ends at
   L(strcat_strlen_done).  */
	movq	%rdi, %r8
	andq	$(VEC_SIZE * -1), %r8
	VPCMPEQ	(%r8), %VZERO, %VMM(0)
	vpmovmskb %VMM(0), %ecx
	shrxl	%edi, %ecx, %ecx
	testl	%ecx, %ecx
	jnz	L(bsf_and_done_v0)

	VPCMPEQ	VEC_SIZE(%r8), %VZERO, %VMM(0)
	vpmovmskb %VMM(0), %ecx
	leaq	(VEC_SIZE)(%r8), %rdi
	testl	%ecx, %ecx
	jnz	L(bsf_and_done_v0)

	VPCMPEQ	(VEC_SIZE * 1)(%rdi), %VZERO, %VMM(0)
	vpmovmskb %VMM(0), %ecx
	testl	%ecx, %ecx
	jnz	L(bsf_and_done_v1)

	VPCMPEQ	(VEC_SIZE * 2)(%rdi), %VZERO, %VMM(0)
	vpmovmskb %VMM(0), %ecx
	testl	%ecx, %ecx
	jnz	L(bsf_and_done_v2)

	VPCMPEQ	(VEC_SIZE * 3)(%rdi), %VZERO, %VMM(0)
	vpmovmskb %VMM(0), %ecx
	testl	%ecx, %ecx
	jnz	L(bsf_and_done_v3)

	orq	$(VEC_SIZE * 4 - 1), %rdi
	.p2align 4,, 8
L(loop_2x_vec):
	VMOVA	(VEC_SIZE * 0 + 1)(%rdi), %VMM(0)
	VPMIN	(VEC_SIZE * 1 + 1)(%rdi), %VMM(0), %VMM(1)
	VMOVA	(VEC_SIZE * 2 + 1)(%rdi), %VMM(2)
	VPMIN	(VEC_SIZE * 3 + 1)(%rdi), %VMM(2), %VMM(3)
	VPMIN	%VMM(1), %VMM(3), %VMM(3)
	VPCMPEQ	%VMM(3), %VZERO, %VMM(3)
	vpmovmskb %VMM(3), %r8d
	subq	$(VEC_SIZE * -4), %rdi
	testl	%r8d, %r8d
	jz	L(loop_2x_vec)

	addq	$(VEC_SIZE * -4 + 1), %rdi

	VPCMPEQ	%VMM(0), %VZERO, %VMM(0)
	vpmovmskb %VMM(0), %ecx
	testl	%ecx, %ecx
	jnz	L(bsf_and_done_v0)

	VPCMPEQ	%VMM(1), %VZERO, %VMM(1)
	vpmovmskb %VMM(1), %ecx
	testl	%ecx, %ecx
	jnz	L(bsf_and_done_v1)

	VPCMPEQ	%VMM(2), %VZERO, %VMM(2)
	vpmovmskb %VMM(2), %ecx
	testl	%ecx, %ecx
	jnz	L(bsf_and_done_v2)

	movl	%r8d, %ecx
L(bsf_and_done_v3):
	addq	$VEC_SIZE, %rdi
L(bsf_and_done_v2):
	bsfl	%ecx, %ecx
	leaq	(VEC_SIZE * 2)(%rdi, %rcx), %rdi
	jmp	L(strcat_strlen_done)

	.p2align 4,, 4
L(bsf_and_done_v1):
	addq	$VEC_SIZE, %rdi
L(bsf_and_done_v0):
	bsfl	%ecx, %ecx
	addq	%rcx, %rdi
L(strcat_strlen_done):
