/* strnlen/wcsnlen optimized with 256-bit EVEX instructions.
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

#include <isa-level.h>
#include <sysdep.h>

#if ISA_SHOULD_BUILD (4)

# ifndef VEC_SIZE
#  include "x86-evex256-vecs.h"
# endif


# ifndef STRNLEN
#  define STRNLEN	__strnlen_evex
# endif

# ifdef USE_AS_WCSLEN
#  define VPCMPEQ	vpcmpeqd
#  define VPCMPNEQ	vpcmpneqd
#  define VPTESTN	vptestnmd
#  define VPTEST	vptestmd
#  define VPMINU	vpminud
#  define CHAR_SIZE	4

# else
#  define VPCMPEQ	vpcmpeqb
#  define VPCMPNEQ	vpcmpneqb
#  define VPTESTN	vptestnmb
#  define VPTEST	vptestmb
#  define VPMINU	vpminub
#  define CHAR_SIZE	1

#  define REG_WIDTH	VEC_SIZE
# endif

# define CHAR_PER_VEC	(VEC_SIZE / CHAR_SIZE)

# include "reg-macros.h"

# if CHAR_PER_VEC == 32
#  define SUB_SHORT(imm, reg)	subb $(imm), %VGPR_SZ(reg, 8)
# else
#  define SUB_SHORT(imm, reg)	subl $(imm), %VGPR_SZ(reg, 32)
# endif



# if CHAR_PER_VEC == 64
#  define FALLTHROUGH_RETURN_OFFSET	(CHAR_PER_VEC * 3)
# else
#  define FALLTHROUGH_RETURN_OFFSET	(CHAR_PER_VEC * 2)
# endif


# define XZERO	VMM_128(0)
# define VZERO	VMM(0)
# define PAGE_SIZE	4096

	.section SECTION(.text), "ax", @progbits
ENTRY_P2ALIGN (STRNLEN, 6)
	/* Check zero length.  */
	test	%RSI_LP, %RSI_LP
	jz	L(zero)
# ifdef __ILP32__
	/* Clear the upper 32 bits.  */
	movl	%esi, %esi
# endif

	movl	%edi, %eax
	vpxorq	%XZERO, %XZERO, %XZERO
	andl	$(PAGE_SIZE - 1), %eax
	cmpl	$(PAGE_SIZE - VEC_SIZE), %eax
	ja	L(cross_page_boundary)

	/* Check the first VEC_SIZE bytes.  Each bit in K0 represents a
	   null byte.  */
	VPCMPEQ	(%rdi), %VZERO, %k0

	KMOV	%k0, %VRCX
	movq	%rsi, %rax

	/* If src (rcx) is zero, bsf does not change the result.  NB:
	   Must use 64-bit bsf here so that upper bits of len are not
	   cleared.  */
	bsfq	%rcx, %rax
	/* If rax > CHAR_PER_VEC then rcx must have been zero (no null
	   CHAR) and rsi must be > CHAR_PER_VEC.  */
	cmpq	$CHAR_PER_VEC, %rax
	ja	L(more_1x_vec)
	/* Check if first match in bounds.  */
	cmpq	%rax, %rsi
	cmovb	%esi, %eax
	ret


# if CHAR_PER_VEC != 32
	.p2align 4,, 2
L(zero):
L(max_0):
	movl	%esi, %eax
	ret
# endif

	/* Aligned more for strnlen compares remaining length vs 2 *
	   CHAR_PER_VEC, 4 * CHAR_PER_VEC, and 8 * CHAR_PER_VEC before
	   going to the loop.  */
	.p2align 4,, 10
L(more_1x_vec):
L(cross_page_continue):
	/* Compute number of words checked after aligning.  */
# ifdef USE_AS_WCSLEN
	/* Need to compute directly for wcslen as CHAR_SIZE * rsi can
	   overflow.  */
	movq	%rdi, %rax
	andq	$(VEC_SIZE * -1), %rdi
	subq	%rdi, %rax
	sarq	$2, %rax
	leaq	-(CHAR_PER_VEC * 1)(%rax, %rsi), %rax
# else
	leaq	(VEC_SIZE * -1)(%rsi, %rdi), %rax
	andq	$(VEC_SIZE * -1), %rdi
	subq	%rdi, %rax
# endif


	VPCMPEQ	VEC_SIZE(%rdi), %VZERO, %k0

	cmpq	$(CHAR_PER_VEC * 2), %rax
	ja	L(more_2x_vec)

L(last_2x_vec_or_less):
	KMOV	%k0, %VRDX
	test	%VRDX, %VRDX
	jnz	L(last_vec_check)

	/* Check the end of data.  */
	SUB_SHORT (CHAR_PER_VEC, rax)
	jbe	L(max_0)
	VPCMPEQ	(VEC_SIZE * 2)(%rdi), %VZERO, %k0
	KMOV	%k0, %VRDX
	test	%VRDX, %VRDX
	jz	L(max_0)
	/* Best place for LAST_VEC_CHECK if ZMM.  */
	.p2align 4,, 8
L(last_vec_check):
	bsf	%VRDX, %VRDX
	sub	%eax, %edx
	lea	(%rsi, %rdx), %eax
	cmovae	%esi, %eax
	ret

# if CHAR_PER_VEC == 32
	.p2align 4,, 2
L(zero):
L(max_0):
	movl	%esi, %eax
	ret
# endif

	.p2align 4,, 8
L(last_4x_vec_or_less):
	addl	$(CHAR_PER_VEC * -4), %eax
	VPCMPEQ	(VEC_SIZE * 5)(%rdi), %VZERO, %k0
	subq	$(VEC_SIZE * -4), %rdi
	cmpl	$(CHAR_PER_VEC * 2), %eax
	jbe	L(last_2x_vec_or_less)

	.p2align 4,, 6
L(more_2x_vec):
	/* Remaining length >= 2 * CHAR_PER_VEC so do VEC0/VEC1 without
	   rechecking bounds.  */

	KMOV	%k0, %VRDX

	test	%VRDX, %VRDX
	jnz	L(first_vec_x1)

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

	subl	$(CHAR_PER_VEC), %eax
	jbe	L(max_1)

	VPCMPEQ	(VEC_SIZE * 4)(%rdi), %VZERO, %k0
	KMOV	%k0, %VRDX

	test	%VRDX, %VRDX
	jnz	L(last_vec_check)
L(max_1):
	movl	%esi, %eax
	ret

	.p2align 4,, 3
L(first_vec_x2):
# if VEC_SIZE == 64
	/* If VEC_SIZE == 64 we can fit logic for full return label in
	   spare bytes before next cache line.  */
	bsf	%VRDX, %VRDX
	sub	%eax, %esi
	leal	(CHAR_PER_VEC * 1)(%rsi, %rdx), %eax
	ret
	.p2align 4,, 6
# else
	addl	$CHAR_PER_VEC, %esi
# endif
L(first_vec_x1):
	bsf	%VRDX, %VRDX
	sub	%eax, %esi
	leal	(CHAR_PER_VEC * 0)(%rsi, %rdx), %eax
	ret


	.p2align 4,, 6
L(first_vec_x4):
# if VEC_SIZE == 64
	/* If VEC_SIZE == 64 we can fit logic for full return label in
	   spare bytes before next cache line.  */
	bsf	%VRDX, %VRDX
	sub	%eax, %esi
	leal	(CHAR_PER_VEC * 3)(%rsi, %rdx), %eax
	ret
	.p2align 4,, 6
# else
	addl	$CHAR_PER_VEC, %esi
# endif
L(first_vec_x3):
	bsf	%VRDX, %VRDX
	sub	%eax, %esi
	leal	(CHAR_PER_VEC * 2)(%rsi, %rdx), %eax
	ret

	.p2align 4,, 5
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
# ifdef USE_AS_WCSLEN
	/* Need to compute directly for wcslen as CHAR_SIZE * rsi can
	   overflow.  */
	leaq	(VEC_SIZE * -3)(%rdi), %rdx
# else
	leaq	(VEC_SIZE * -3)(%rdi, %rax), %rax
# endif

	subq	$(VEC_SIZE * -1), %rdi

	/* Align data to VEC_SIZE * 4.  */
# if VEC_SIZE == 64
	/* Saves code size.  No evex512 processor has partial register
	   stalls.  If that change this can be replaced with `andq
	   $-(VEC_SIZE * 4), %rdi`.  */
	xorb	%dil, %dil
# else
	andq	$-(VEC_SIZE * 4), %rdi
# endif

# ifdef USE_AS_WCSLEN
	subq	%rdi, %rdx
	sarq	$2, %rdx
	addq	%rdx, %rax
# else
	subq	%rdi, %rax
# endif
	/* Compare 4 * VEC at a time forward.  */
	.p2align 4,, 11
L(loop_4x_vec):
	VMOVA	(VEC_SIZE * 4)(%rdi), %VMM(1)
	VPMINU	(VEC_SIZE * 5)(%rdi), %VMM(1), %VMM(2)
	VMOVA	(VEC_SIZE * 6)(%rdi), %VMM(3)
	VPMINU	(VEC_SIZE * 7)(%rdi), %VMM(3), %VMM(4)
	VPTESTN	%VMM(2), %VMM(2), %k0
	VPTESTN	%VMM(4), %VMM(4), %k2
	subq	$-(VEC_SIZE * 4), %rdi
	/* Break if at end of length.  */
	subq	$(CHAR_PER_VEC * 4), %rax
	jbe	L(loop_len_end)


	KORTEST %k0, %k2
	jz	L(loop_4x_vec)


L(loop_last_4x_vec):
	movq	%rsi, %rcx
	subq	%rax, %rsi
	VPTESTN	%VMM(1), %VMM(1), %k1
	KMOV	%k1, %VRDX
	test	%VRDX, %VRDX
	jnz	L(last_vec_x0)

	KMOV	%k0, %VRDX
	test	%VRDX, %VRDX
	jnz	L(last_vec_x1)

	VPTESTN	%VMM(3), %VMM(3), %k0

	/* Separate logic for VEC_SIZE == 64 and VEC_SIZE == 32 for
	   returning last 2x VEC. For VEC_SIZE == 64 we test each VEC
	   individually, for VEC_SIZE == 32 we combine them in a single
	   64-bit GPR.  */
# if CHAR_PER_VEC == 64
	KMOV	%k0, %VRDX
	test	%VRDX, %VRDX
	jnz	L(last_vec_x2)
	KMOV	%k2, %VRDX
# else
	/* We can only combine last 2x VEC masks if CHAR_PER_VEC <= 32.
	 */
	kmovd	%k2, %edx
	kmovd	%k0, %eax
	salq	$CHAR_PER_VEC, %rdx
	orq	%rax, %rdx
# endif

	/* first_vec_x3 for strlen-ZMM and first_vec_x2 for strlen-YMM.
	 */
	bsfq	%rdx, %rdx
	leaq	(FALLTHROUGH_RETURN_OFFSET - CHAR_PER_VEC * 4)(%rsi, %rdx), %rax
	cmpq	%rax, %rcx
	cmovb	%rcx, %rax
	ret

	/* Handle last 4x VEC after loop. All VECs have been loaded.  */
	.p2align 4,, 4
L(loop_len_end):
	KORTEST %k0, %k2
	jnz	L(loop_last_4x_vec)
	movq	%rsi, %rax
	ret


# if CHAR_PER_VEC == 64
	/* Since we can't combine the last 2x VEC for VEC_SIZE == 64
	   need return label for it.  */
	.p2align 4,, 8
L(last_vec_x2):
	bsf	%VRDX, %VRDX
	leaq	(CHAR_PER_VEC * -2)(%rsi, %rdx), %rax
	cmpq	%rax, %rcx
	cmovb	%rcx, %rax
	ret
# endif


	.p2align 4,, 10
L(last_vec_x1):
	addq	$CHAR_PER_VEC, %rsi
L(last_vec_x0):
	bsf	%VRDX, %VRDX
	leaq	(CHAR_PER_VEC * -4)(%rsi, %rdx), %rax
	cmpq	%rax, %rcx
	cmovb	%rcx, %rax
	ret


	.p2align 4,, 8
L(cross_page_boundary):
	/* Align data to VEC_SIZE.  */
	movq	%rdi, %rcx
	andq	$-VEC_SIZE, %rcx
	VPCMPEQ	(%rcx), %VZERO, %k0

	KMOV	%k0, %VRCX
# ifdef USE_AS_WCSLEN
	shrl	$2, %eax
	andl	$(CHAR_PER_VEC - 1), %eax
# endif
	shrx	%VRAX, %VRCX, %VRCX

	negl	%eax
	andl	$(CHAR_PER_VEC - 1), %eax
	movq	%rsi, %rdx
	bsf	%VRCX, %VRDX
	cmpq	%rax, %rdx
	ja	L(cross_page_continue)
	movl	%edx, %eax
	cmpq	%rdx, %rsi
	cmovb	%esi, %eax
	ret
END (STRNLEN)
#endif
