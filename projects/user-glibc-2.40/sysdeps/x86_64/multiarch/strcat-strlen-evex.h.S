/* strlen used for beginning of str{n}cat using EVEX 256/512.
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


/* NOTE: This file is meant to be included by strcat-evex or
   strncat-evex and does not standalone.  Before including %rdi
   must be saved in %rax.  */


/* Simple strlen implementation that ends at
   L(strcat_strlen_done).  */
	vpxorq	%VZERO_128, %VZERO_128, %VZERO_128
	movq	%rdi, %r8
	andq	$(VEC_SIZE * -1), %r8
	VPCMPEQ	(%r8), %VZERO, %k0
	KMOV	%k0, %VRCX
#ifdef USE_AS_WCSCPY
	subl	%r8d, %edi
	shrl	$2, %edi
#endif
	shrx	%VRDI, %VRCX, %VRCX
#ifdef USE_AS_WCSCPY
	movq	%rax, %rdi
#endif
	test	%VRCX, %VRCX
	jnz	L(bsf_and_done_v0)


	VPCMPEQ	VEC_SIZE(%r8), %VZERO, %k0
	KMOV	%k0, %VRCX
	leaq	(VEC_SIZE)(%r8), %rdi
	test	%VRCX, %VRCX
	jnz	L(bsf_and_done_v0)

	VPCMPEQ	(VEC_SIZE * 2)(%r8), %VZERO, %k0
	KMOV	%k0, %VRCX
	test	%VRCX, %VRCX
	jnz	L(bsf_and_done_v1)

	VPCMPEQ	(VEC_SIZE * 3)(%r8), %VZERO, %k0
	KMOV	%k0, %VRCX
	test	%VRCX, %VRCX
	jnz	L(bsf_and_done_v2)

	VPCMPEQ	(VEC_SIZE * 4)(%r8), %VZERO, %k0
	KMOV	%k0, %VRCX
	test	%VRCX, %VRCX
	jnz	L(bsf_and_done_v3)

	andq	$-(VEC_SIZE * 4), %rdi
	.p2align 4,, 8
L(loop_2x_vec):
	VMOVA	(VEC_SIZE * 4)(%rdi), %VMM(0)
	VPMIN	(VEC_SIZE * 5)(%rdi), %VMM(0), %VMM(1)
	VMOVA	(VEC_SIZE * 6)(%rdi), %VMM(2)
	VPMIN	(VEC_SIZE * 7)(%rdi), %VMM(2), %VMM(3)
	VPTESTN	%VMM(1), %VMM(1), %k1
	VPTESTN	%VMM(3), %VMM(3), %k3
	subq	$(VEC_SIZE * -4), %rdi
	KORTEST	%k1, %k3
	jz	L(loop_2x_vec)

	VPTESTN	%VMM(0), %VMM(0), %k0
	KMOV	%k0, %VRCX
	test	%VRCX, %VRCX
	jnz	L(bsf_and_done_v0)

	KMOV	%k1, %VRCX
	test	%VRCX, %VRCX
	jnz	L(bsf_and_done_v1)

	VPTESTN	%VMM(2), %VMM(2), %k0
	KMOV	%k0, %VRCX
	test	%VRCX, %VRCX
	jnz	L(bsf_and_done_v2)

	KMOV	%k3, %VRCX
L(bsf_and_done_v3):
	addq	$VEC_SIZE, %rdi
L(bsf_and_done_v2):
	bsf	%VRCX, %VRCX
	leaq	(VEC_SIZE * 2)(%rdi, %rcx, CHAR_SIZE), %rdi
	jmp	L(strcat_strlen_done)

	.p2align 4,, 4
L(bsf_and_done_v1):
	addq	$VEC_SIZE, %rdi
L(bsf_and_done_v0):
	bsf	%VRCX, %VRCX
#ifdef USE_AS_WCSCPY
	leaq	(%rdi, %rcx, CHAR_SIZE), %rdi
#else
	addq	%rcx, %rdi
#endif
L(strcat_strlen_done):
