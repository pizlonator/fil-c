/* Placeholder function, not used by any processor at the moment.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

/* UNUSED. Exists purely as reference implementation.  */

#include <isa-level.h>

#if ISA_SHOULD_BUILD (4)

# include <sysdep.h>

# ifdef USE_AS_WMEMCHR
#  define CHAR_SIZE	4
#  define VPBROADCAST   vpbroadcastd
#  define VPCMPEQ	vpcmpeqd
#  define VPCMPNE	vpcmpneqd
#  define VPMINU	vpminud
#  define VPTESTNM	vptestnmd
# else
#  define CHAR_SIZE	1
#  define VPBROADCAST   vpbroadcastb
#  define VPCMPEQ	vpcmpeqb
#  define VPCMPNE	vpcmpneqb
#  define VPMINU	vpminub
#  define VPTESTNM	vptestnmb
# endif

# define PAGE_SIZE	4096
# define CHAR_PER_VEC	(VEC_SIZE / CHAR_SIZE)

	.section SECTION(.text), "ax", @progbits
/* Aligning entry point to 64 byte, provides better performance for
   one vector length string.  */
ENTRY_P2ALIGN (MEMCHR, 6)
# ifndef USE_AS_RAWMEMCHR
	/* Check for zero length.  */
	test	%RDX_LP, %RDX_LP
	jz	L(zero)

#  ifdef __ILP32__
	/* Clear the upper 32 bits.  */
	movl	%edx, %edx
#  endif
# endif

	/* Broadcast CHAR to VMM(1).  */
	VPBROADCAST %esi, %VMM(1)
	movl	%edi, %eax
	andl	$(PAGE_SIZE - 1), %eax
	cmpl	$(PAGE_SIZE - VEC_SIZE), %eax
	ja	L(page_cross)

	/* Compare [w]char for null, mask bit will be set for match.  */
	VPCMPEQ	(%rdi), %VMM(1), %k0

	KMOV	%k0, %VRCX
# ifndef USE_AS_RAWMEMCHR
	mov	%rdx, %rsi
	/* Need to use bsfq here as upper 32 bit of rsi may zero out
	   for 'bsf %ecx, %esi', if %ecx is 0.  */
	bsfq	%rcx, %rsi
	cmp	$CHAR_PER_VEC, %rsi
	ja	L(align_more)
#  ifdef USE_AS_WMEMCHR
	leaq	(%rdi, %rsi, CHAR_SIZE), %rdi
#  else
	addq	%rsi, %rdi
#  endif
	xor	%eax, %eax
	cmp	%rsi, %rdx
	cmova	%rdi, %rax
# else
	bsf     %VRCX, %VRAX
	jz	L(align_more)
	add	%rdi, %rax
# endif
	ret

	.p2align 5,,5
L(page_cross):
	movl	%eax, %ecx
	andl	$(VEC_SIZE - 1), %ecx
# ifdef USE_AS_WMEMCHR
	shrl	$2, %ecx
# endif
	xorq	%rdi, %rax
	VPCMPEQ (PAGE_SIZE - VEC_SIZE)(%rax), %VMM(1), %k0
	KMOV    %k0, %VRSI
	shr	%cl, %VRSI
# ifndef USE_AS_RAWMEMCHR
	jnz	L(page_cross_end)
	movl	$CHAR_PER_VEC, %eax
	sub	%ecx, %eax
	cmp	%rax, %rdx
	ja	L(align_more)
# else
	jz	L(align_more)
# endif

L(page_cross_end):
# ifndef USE_AS_RAWMEMCHR
	bsf	%VRSI, %VRCX
	jz	L(zero)
	leaq	(%rdi, %rcx, CHAR_SIZE), %rdi
	xor	%eax, %eax
	cmp	%rcx, %rdx
	cmova	%rdi, %rax
# else
	bsf	%VRSI, %VRAX
	add	%rdi, %rax
# endif
	ret

# ifndef USE_AS_RAWMEMCHR
L(zero):
	xorl	%eax, %eax
	ret
# endif

L(ret_vec_x2):
	subq	$-VEC_SIZE, %rdi
L(ret_vec_x1):
	bsf     %VRAX, %VRAX
# ifndef USE_AS_RAWMEMCHR
	cmp	%rax, %rdx
	jbe	L(zero)
# endif
# ifdef USE_AS_WMEMCHR
	leaq	(%rdi, %rax, CHAR_SIZE), %rax
# else
	add	%rdi, %rax
# endif
	ret

	.p2align 5,,5
L(align_more):
# ifndef USE_AS_RAWMEMCHR
	mov	%rdi, %rax
# endif
	subq	$-VEC_SIZE, %rdi
	/* Align rdi to VEC_SIZE.  */
	andq	$-VEC_SIZE, %rdi

# ifndef USE_AS_RAWMEMCHR
	subq	%rdi, %rax
#  ifdef USE_AS_WMEMCHR
	sar	$2, %rax
#  endif
	addq	%rax, %rdx
# endif

	/* Loop unroll 4 times for 4 vector loop.  */
	VPCMPEQ	(%rdi), %VMM(1), %k0

	KMOV	%k0, %VRAX
	test	%VRAX, %VRAX
	jnz	L(ret_vec_x1)

# ifndef USE_AS_RAWMEMCHR
	subq	$CHAR_PER_VEC, %rdx
	jbe	L(zero)
# endif

	VPCMPEQ	VEC_SIZE(%rdi), %VMM(1), %k0

	KMOV	%k0, %VRAX
	test	%VRAX, %VRAX
	jnz	L(ret_vec_x2)

# ifndef USE_AS_RAWMEMCHR
	subq	$CHAR_PER_VEC, %rdx
	jbe	L(zero)
# endif

	VPCMPEQ	(VEC_SIZE * 2)(%rdi), %VMM(1), %k0

	KMOV	%k0, %VRAX
	test	%VRAX, %VRAX
	jnz	L(ret_vec_x3)

# ifndef USE_AS_RAWMEMCHR
	subq	$CHAR_PER_VEC, %rdx
	jbe	L(zero)
# endif

	VPCMPEQ	(VEC_SIZE * 3)(%rdi), %VMM(1), %k0

	KMOV	%k0, %VRAX
	test	%VRAX, %VRAX
	jnz	L(ret_vec_x4)

# ifndef USE_AS_RAWMEMCHR
	subq	$CHAR_PER_VEC, %rdx
	jbe	L(zero)
	/* Save pointer to find alignment adjustment.  */
	movq	%rdi, %rax
# endif
	/* Align address to VEC_SIZE * 4 for loop.  */
	andq	$-(VEC_SIZE * 4), %rdi

	/* Add alignment difference to rdx.  */
# ifndef USE_AS_RAWMEMCHR
	subq	%rdi, %rax
#  ifdef USE_AS_WMEMCHR
	shr	$2, %VRAX
#  endif
	addq	%rax, %rdx
# endif

	/* 4 vector loop.  */
	.p2align 5,,11
L(loop):

	VPCMPNE	(VEC_SIZE * 4)(%rdi), %VMM(1), %k1
	vpxorq  (VEC_SIZE * 5)(%rdi), %VMM(1), %VMM(2)
	vpxorq  (VEC_SIZE * 6)(%rdi), %VMM(1), %VMM(3)
	VPCMPEQ (VEC_SIZE * 7)(%rdi), %VMM(1), %k3
	VPMINU  %VMM(2), %VMM(3), %VMM(3){%k1}{z}
	VPTESTNM %VMM(3), %VMM(3), %k2

	subq	$-(VEC_SIZE * 4), %rdi
	KORTEST	%k2, %k3
# ifdef USE_AS_RAWMEMCHR
	jz	L(loop)
# else
	jnz	L(loopend)
	subq	$(CHAR_PER_VEC * 4), %rdx
	ja	L(loop)
L(zero_2):
	xor	%eax, %eax
	ret
# endif

L(loopend):
	VPCMPEQ	(%rdi), %VMM(1), %k1
	KMOV	%k1, %VRAX
	test	%VRAX, %VRAX
	jnz	L(ret_vec_x1)

# ifndef USE_AS_RAWMEMCHR
	subq	$CHAR_PER_VEC, %rdx
	jbe	L(zero_2)
# endif

	VPCMPEQ	VEC_SIZE(%rdi), %VMM(1), %k1
	KMOV	%k1, %VRAX
	test	%VRAX, %VRAX
	jnz	L(ret_vec_x2)

# ifndef USE_AS_RAWMEMCHR
	subq	$CHAR_PER_VEC, %rdx
	jbe	L(zero_2)
# endif

	VPCMPEQ	(VEC_SIZE * 2)(%rdi), %VMM(1), %k1
	KMOV	%k1, %VRAX
	test	%VRAX, %VRAX
	jnz	L(ret_vec_x3)

# ifndef USE_AS_RAWMEMCHR
	subq	$CHAR_PER_VEC, %rdx
	jbe	L(zero_2)
# endif

	/* At this point null [w]char must be in the fourth vector so no
	   need to check.  */
	KMOV	%k3, %VRAX

L(ret_vec_x4):
	bsf	%VRAX, %VRAX
# ifndef USE_AS_RAWMEMCHR
	cmp	%rax, %rdx
	jbe	L(zero)
# endif
	leaq	(VEC_SIZE * 3)(%rdi, %rax, CHAR_SIZE), %rax
	ret

	.p2align 5,,5
L(ret_vec_x3):
	bsf	%VRAX, %VRAX
# ifndef USE_AS_RAWMEMCHR
	cmp	%rax, %rdx
	jbe	L(zero)
# endif
	leaq	(VEC_SIZE * 2)(%rdi, %rax, CHAR_SIZE), %rax
	ret

END (MEMCHR)
#endif
