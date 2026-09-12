/* strnlen/wcsnlen optimized with 256/512-bit EVEX instructions.
   Copyright (C) 2022-2026 Free Software Foundation, Inc.
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

#if ISA_SHOULD_BUILD (4)

# include <sysdep.h>

#ifdef USE_AS_WCSLEN
# define VPCMPEQ	vpcmpeqd
# define VPTESTN	vptestnmd
# define VPMINU	vpminud
# define CHAR_SIZE	4
#else
# define VPCMPEQ	vpcmpeqb
# define VPTESTN	vptestnmb
# define VPMINU	vpminub
# define CHAR_SIZE	1
#endif

#define XZERO	VMM_128(0)
#define VZERO	VMM(0)
#define PAGE_SIZE	4096
#define CHAR_PER_VEC	(VEC_SIZE / CHAR_SIZE)

#if CHAR_PER_VEC == 32
# define SUB_SHORT(imm, reg)	subb $(imm), %VGPR_SZ(reg, 8)
#else
# define SUB_SHORT(imm, reg)	subl $(imm), %VGPR_SZ(reg, 32)
#endif

#ifdef USE_AS_WCSLEN
/* For wide-character, we care more about limitting code size
   than optimally aligning targets, so just cap nop padding
   reasonably low.  */
# define P2ALIGN(...)	.p2align 4,, 6
# define P2ALIGN_CLAMPED(...)	P2ALIGN(__VA_ARGS__)
#else
# define P2ALIGN(x)	.p2align x
# define P2ALIGN_CLAMPED(x, y)	.p2align x,, y
#endif

	.section SECTION(.text), "ax", @progbits
	/* Aligning entry point to 64 byte, provides better performance for
	   one vector length string.  */
ENTRY_P2ALIGN(STRNLEN, 6)
	/* rdi is pointer to array, rsi is the upper limit.  */

	/* Check zero length.  */
	test	%RSI_LP, %RSI_LP
	jz	L(zero)

#ifdef __ILP32__
	/* Clear the upper 32 bits.  */
	movl	%esi, %esi
#endif

	vpxorq	%XZERO, %XZERO, %XZERO

	/* Check that we won't cross a page boundary with our first load.  */
	movl	%edi, %eax
	shll	$20, %eax
	cmpl	$((PAGE_SIZE - VEC_SIZE) << 20), %eax
	ja	L(crosses_page_boundary)

	/* Check the first VEC_SIZE bytes.  Each bit in K0 represents a
	   null byte.  */
	VPCMPEQ	(%rdi), %VZERO, %k0
	KMOV	%k0, %VRCX

	/* If src (rcx) is zero, bsf does not change the result.  NB:
	   Must use 64-bit bsf here so that upper bits of len are not
	   cleared.  */
	movq	%rsi, %rax
	bsfq	%rcx, %rax

	/* If rax > CHAR_PER_VEC then rcx must have been zero (no null
	   CHAR) and rsi must be > CHAR_PER_VEC.  */
	cmpq	$CHAR_PER_VEC, %rax
	ja	L(more_1x_vec)

	/* Check if first match in bounds.  */
	cmpq	%rax, %rsi
	cmovb	%esi, %eax
	ret

#if VEC_SIZE == 32
	P2ALIGN_CLAMPED(4, 2)
L(zero):
L(max_0):
	movl	%esi, %eax
	ret
#endif

	P2ALIGN_CLAMPED(4, 10)
L(more_1x_vec):
L(cross_page_continue):
	/* After this calculation, rax stores the number of elements
	   left to be processed The complexity comes from the fact some
	   elements get read twice due to alignment and we need to be
	   sure we don't count them twice (else, it would just be rsi -
	   CHAR_PER_VEC).  */

#ifdef USE_AS_WCSLEN
	/* Need to compute directly for wcslen as CHAR_SIZE * rsi can
	   overflow.  */
	movq	%rdi, %rax
	andq	$(VEC_SIZE * -1), %rdi
	subq	%rdi, %rax
	sarq	$2, %rax
	leaq	-(CHAR_PER_VEC * 1)(%rax, %rsi), %rax
#else
	/* Calculate ptr + N - VEC_SIZE, then mask off the low bits,
	   then subtract ptr to get the new aligned limit value.  */
	leaq	(VEC_SIZE * -1)(%rsi, %rdi), %rax
	andq	$(VEC_SIZE * -1), %rdi
	subq	%rdi, %rax
#endif

	VPCMPEQ	VEC_SIZE(%rdi), %VZERO, %k0

	/* Checking here is faster for 256-bit but not 512-bit */
#if VEC_SIZE == 0
	KMOV	%k0, %VRDX
	test	%VRDX, %VRDX
	jnz	L(last_vec_check)
#endif

	cmpq	$(CHAR_PER_VEC * 2), %rax
	ja	L(more_2x_vec)

L(last_2x_vec_or_less):

	/* Checking here is faster for 512-bit but not 256-bit */
#if VEC_SIZE != 0
	KMOV	%k0, %VRDX
	test	%VRDX, %VRDX
	jnz	L(last_vec_check)
#endif

	/* Check for the end of data.  */
	SUB_SHORT (CHAR_PER_VEC, rax)
	jbe	L(max_0)

	/* Check the final remaining vector.  */
	VPCMPEQ	(VEC_SIZE * 2)(%rdi), %VZERO, %k0
	KMOV	%k0, %VRDX
	test	%VRDX, %VRDX
#if VEC_SIZE == 32
	jz	L(max_0)
#else
	jnz	L(last_vec_check)
	P2ALIGN_CLAMPED(4, 2)
L(zero):
L(max_0):
	movl	%esi, %eax
	ret

#endif
	P2ALIGN_CLAMPED(4, 4)
L(last_vec_check):
	bsf	%VRDX, %VRDX
	sub	%eax, %edx
	lea	(%rsi, %rdx), %eax
	cmovae	%esi, %eax
	ret


#if VEC_SIZE == 32
	P2ALIGN_CLAMPED(4, 8)
#endif
L(last_4x_vec_or_less):
	addl	$(CHAR_PER_VEC * -4), %eax
	VPCMPEQ	(VEC_SIZE * 5)(%rdi), %VZERO, %k0

#if VEC_SIZE == 64
	KMOV	%k0, %VRDX
	test	%VRDX, %VRDX
	jnz	L(last_vec_check)
#endif

	subq	$(VEC_SIZE * -4), %rdi
	cmpl	$(CHAR_PER_VEC * 2), %eax
	jbe	L(last_2x_vec_or_less)

	P2ALIGN_CLAMPED(4, 6)
L(more_2x_vec):
	/* Remaining length >= 2 * CHAR_PER_VEC so do VEC0/VEC1 without
	   rechecking bounds.  */

	/* Already checked in 256-bit case */
#if VEC_SIZE != 0
	KMOV	%k0, %VRDX

	test	%VRDX, %VRDX
	jnz	L(first_vec_x1)
#endif

	VPCMPEQ	(VEC_SIZE * 2)(%rdi), %VZERO, %k0
	KMOV	%k0, %VRDX

	test	%VRDX, %VRDX
	jnz	L(first_vec_x2)

	cmpq	$(CHAR_PER_VEC * 4), %rax
	ja	L(more_4x_vec)


	VPCMPEQ	(VEC_SIZE * 3)(%rdi), %VZERO, %k0
	KMOV	%k0, %VRDX
	addl	$(CHAR_PER_VEC * -2), %eax
	test	%VRDX, %VRDX
	jnz	L(last_vec_check)

	subb	$(CHAR_PER_VEC), %al
	jbe	L(max_1)

	VPCMPEQ	(VEC_SIZE * 4)(%rdi), %VZERO, %k0
	KMOV	%k0, %VRDX

	test	%VRDX, %VRDX
	jnz	L(last_vec_check)
L(max_1):
	movl	%esi, %eax
	ret


	P2ALIGN_CLAMPED(4, 14)
L(first_vec_x2):
#if VEC_SIZE == 64
	/* If VEC_SIZE == 64 we can fit logic for full return label in
	   spare bytes before next cache line.  */
	bsf	%VRDX, %VRDX
	sub	%eax, %esi
	leal	(CHAR_PER_VEC * 1)(%rsi, %rdx), %eax
	ret
	P2ALIGN_CLAMPED(4, 6)
#else
	addl	$CHAR_PER_VEC, %esi
#endif
L(first_vec_x1):
	bsf	%VRDX, %VRDX
	sub	%eax, %esi
	leal	(CHAR_PER_VEC * 0)(%rsi, %rdx), %eax
	ret

#if VEC_SIZE == 64
	P2ALIGN_CLAMPED(4, 6)
L(first_vec_x4):
# if VEC_SIZE == 64
	/* If VEC_SIZE == 64 we can fit logic for full return label in
	   spare bytes before next cache line.  */
	bsf	%VRDX, %VRDX
	sub	%eax, %esi
	leal	(CHAR_PER_VEC * 3)(%rsi, %rdx), %eax
	ret
	P2ALIGN_CLAMPED(4, 6)
# else
	addl	$CHAR_PER_VEC, %esi
# endif
L(first_vec_x3):
	bsf	%VRDX, %VRDX
	sub	%eax, %esi
	leal	(CHAR_PER_VEC * 2)(%rsi, %rdx), %eax
	ret
#endif

	P2ALIGN_CLAMPED(6, 20)
L(more_4x_vec):
	VPCMPEQ	(VEC_SIZE * 3)(%rdi), %VZERO, %k0
	KMOV	%k0, %VRDX
	test	%VRDX, %VRDX
	jnz	L(first_vec_x3)

	VPCMPEQ	(VEC_SIZE * 4)(%rdi), %VZERO, %k0
	KMOV	%k0, %VRDX
	test	%VRDX, %VRDX
	jnz	L(first_vec_x4)

	/* Check if at last VEC_SIZE * 4 length before aligning for the
	   loop.  */
	cmpq	$(CHAR_PER_VEC * 8), %rax
	jbe	L(last_4x_vec_or_less)


	/* Compute number of words checked after aligning.  */
#ifdef USE_AS_WCSLEN
	/* Need to compute directly for wcslen as CHAR_SIZE * rsi can
	   overflow.  */
	leaq	(VEC_SIZE * -3)(%rdi), %rdx
#else
	leaq	(VEC_SIZE * -3)(%rdi, %rax), %rax
#endif

	subq	$(VEC_SIZE * -1), %rdi

	/* Align data to VEC_SIZE * 4.  */
#if VEC_SIZE == 64
	/* Saves code size.  No evex512 processor has partial register
	   stalls.  If that change this can be replaced with `andq
	   $-(VEC_SIZE * 4), %rdi`.  */
	xorb	%dil, %dil
#else
	andq	$-(VEC_SIZE * 4), %rdi
#endif

#ifdef USE_AS_WCSLEN
	subq	%rdi, %rdx
	sarq	$2, %rdx
	addq	%rdx, %rax
#else
	subq	%rdi, %rax
#endif

	// mov     %rdi, %rdx

	P2ALIGN(6)
L(loop):
	/* VPMINU and VPCMP combination provide better performance as
	   compared to alternative combinations.  */
	VMOVA	(VEC_SIZE * 4)(%rdi), %VMM(1)
	VPMINU	(VEC_SIZE * 5)(%rdi), %VMM(1), %VMM(2)
	VMOVA	(VEC_SIZE * 6)(%rdi), %VMM(3)
	VPMINU	(VEC_SIZE * 7)(%rdi), %VMM(3), %VMM(4)

	VPTESTN	%VMM(2), %VMM(2), %k0
	VPTESTN	%VMM(4), %VMM(4), %k1

	subq	$-(VEC_SIZE * 4), %rdi
	KORTEST	%k0, %k1

	jnz	L(loopend)
	subq	$(CHAR_PER_VEC * 4), %rax
	ja	L(loop)
	mov	%rsi, %rax
	ret


#if VEC_SIZE == 32
	P2ALIGN_CLAMPED(4, 6)
L(first_vec_x4):
# if VEC_SIZE == 64
	/* If VEC_SIZE == 64 we can fit logic for full return label in
	   spare bytes before next cache line.  */
	bsf	%VRDX, %VRDX
	sub	%eax, %esi
	leal	(CHAR_PER_VEC * 3)(%rsi, %rdx), %eax
	ret
	P2ALIGN_CLAMPED(4, 6)
# else
	addl	$CHAR_PER_VEC, %esi
# endif
L(first_vec_x3):
	bsf	%VRDX, %VRDX
	sub	%eax, %esi
	leal	(CHAR_PER_VEC * 2)(%rsi, %rdx), %eax
	ret
#endif


	P2ALIGN_CLAMPED(4, 11)
L(loopend):
	/* We found a null terminator in one of the 4 vectors.  */

	/* Check the first vector.  */
	movq	%rax, %r8
	VPTESTN	%VMM(1), %VMM(1), %k2
	KMOV	%k2, %VRCX
	bsf	%rcx, %r8

	cmpq	$(CHAR_PER_VEC), %r8
	jbe	L(end_vec)

	/* Check the second vector.  */
	subq	$(CHAR_PER_VEC), %rax
	movq	%rax, %r8
	KMOV	%k0, %VRCX
	bsf	%rcx, %r8

	cmpq	$(CHAR_PER_VEC), %r8
	jbe	L(end_vec)

	/* Check the third vector.  */
	subq	$(CHAR_PER_VEC), %rax
	movq	%rax, %r8
	VPTESTN	%VMM(3), %VMM(3), %k2
	KMOV	%k2, %VRCX
	bsf	%rcx, %r8

	cmpq	$(CHAR_PER_VEC), %r8
	jbe	L(end_vec)

	/* It is in the fourth vector.  */
	subq	$(CHAR_PER_VEC), %rax
	movq	%rax, %r8
	KMOV	%k1, %VRCX
	bsf	%rcx, %r8

	P2ALIGN_CLAMPED(4, 3)
L(end_vec):
	/* Get the number that has been processed.  */
	movq	%rsi, %rcx
	subq	%rax, %rcx

	/* Add that to the offset we found the null terminator at.  */
	leaq	(%r8, %rcx), %rax

	/* Take the min of that and the limit.  */
	cmpq	%rsi, %rax
	cmovnb	%rsi, %rax
	ret

	P2ALIGN_CLAMPED(4, 11)
L(crosses_page_boundary):
	/* Align data backwards to VEC_SIZE.  */
	shrl	$20, %eax
	movq	%rdi, %rcx
	andq	$-VEC_SIZE, %rcx
	VPCMPEQ	(%rcx), %VZERO, %k0

	KMOV	%k0, %VRCX
#ifdef USE_AS_WCSLEN
	shrl	$2, %eax
	andl	$(CHAR_PER_VEC - 1), %eax
#endif
	/* By this point rax contains number of bytes we need to skip.  */
	shrx	%VRAX, %VRCX, %VRCX

	/* Calculates CHAR_PER_VEC - eax and stores in eax.  */
	negl	%eax
	andl	$(CHAR_PER_VEC - 1), %eax

	movq	%rsi, %rdx
	bsf	%VRCX, %VRDX
	cmpq	%rax, %rdx
	ja	L(cross_page_continue)

	/* The vector had a null terminator or we are at the limit.  */
	movl	%edx, %eax
	cmpq	%rdx, %rsi
	cmovb	%esi, %eax
	ret

END(STRNLEN)
#endif
