/* {wcs|str}ncat  with 256/512-bit EVEX.
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

#if ISA_SHOULD_BUILD (4)

	/* Use evex-masked stores for small sizes. Turned off at the
	   moment.  */
# define USE_EVEX_MASKED_STORE	0

# include <sysdep.h>

# ifndef VEC_SIZE
#  include "x86-evex256-vecs.h"
# endif

# ifndef STRNCAT
#  define STRNCAT	__strncat_evex
# endif


# ifdef USE_AS_WCSCPY
#  define MOVCHAR	movl
#  define VMOVU_MASK	vmovdqu32
#  define VPMIN	vpminud
#  define VPTESTN	vptestnmd
#  define VPTEST	vptestmd
#  define VPCMPEQ	vpcmpeqd
#  define CHAR_SIZE	4

#  define REP_MOVS	rep movsd

#  define VMASK_REG	VR10
#  define FIND_FIRST_ONE(src, dst)	movl $CHAR_PER_VEC, %dst; bsf %src, %dst

#  define USE_WIDE_CHAR
# else
#  define MOVCHAR	movb
#  define VMOVU_MASK	vmovdqu8
#  define VPMIN	vpminub
#  define VPTESTN	vptestnmb
#  define VPTEST	vptestmb
#  define VPCMPEQ	vpcmpeqb
#  define CHAR_SIZE	1

#  define REP_MOVS	rep movsb

#  define VMASK_REG	VRCX
#  define FIND_FIRST_ONE(src, dst)	tzcnt %src, %dst

# endif

# include "strncpy-or-cat-overflow-def.h"

# include "reg-macros.h"


# define VZERO	VMM(7)
# define VZERO_128	VMM_128(7)

# define PAGE_SIZE	4096
# define CHAR_PER_VEC	(VEC_SIZE / CHAR_SIZE)

	.section SECTION(.text), "ax", @progbits
ENTRY(STRNCAT)
# ifdef __ILP32__
	/* Clear the upper 32 bits.  */
	movl	%edx, %edx
# endif

	movq	%rdi, %rax

	/* NB: It's safe to filter out zero-length strings WITHOUT
	   setting null-term. Destination MUST be a null-terminated
	   string so essentially the work is already done.  */
# ifdef USE_AS_WCSCPY
	leaq	-1(%rdx), %rcx
	shrq	$56, %rcx
	jnz	L(zero_len)
# else
	test	%rdx, %rdx
	jle	L(zero_len)
# endif

# include "strcat-strlen-evex.h.S"

	movl	%esi, %ecx
	andl	$(PAGE_SIZE - 1), %ecx
	cmpl	$(PAGE_SIZE - VEC_SIZE), %ecx
	ja	L(page_cross)
L(page_cross_continue):
	VMOVU	(%rsi), %VMM(0)
	VPTESTN	%VMM(0), %VMM(0), %k0

	/* If USE_EVEX_MASK_STORE is enabled then we just handle length
	   <= CHAR_PER_VEC with masked instructions (which have
	   potential for dramatically bad perf if dst splits a page and
	   is not in the TLB).  */
# if USE_EVEX_MASKED_STORE
	KMOV	%k0, %VRCX
	FIND_FIRST_ONE (VRCX, VR8)
	cmpq	%r8, %rdx
	jbe	L(less_1x_vec)

	test	%VRCX, %VRCX
	jz	L(more_1x_vec)

	blsmsk	%VRCX, %VRCX
	KMOV	%VRCX, %k1
	VMOVU_MASK %VMM(0), (%rdi){%k1}
	ret

L(less_1x_vec):
	mov	$-1, %VRCX
	bzhi	%VRDX, %VRCX, %VRCX
	KMOV	%VRCX, %k1
	MOVCHAR	$0, (%rdi, %rdx, CHAR_SIZE)
	VMOVU_MASK %VMM(0), (%rdi){%k1}

	ret
# else
	KMOV	%k0, %VMASK_REG
	/* tzcnt for strncat and `movl $CHAR_PER_VEC, %VRCX; bsf
	   %VMASK_REG, %VRCX` for wcsncat.  */
	FIND_FIRST_ONE (VMASK_REG, VRCX)
	cmpq	%rcx, %rdx
	jbe	L(less_1x_vec)

	/* If there were no zero-CHARs (rcx was zero before
	   FIND_FIRST_ONE), then ecx will be $CHAR_PER_VEC.  */
	cmpl	$CHAR_PER_VEC, %ecx
	je	L(more_1x_vec)

	movl	%ecx, %edx

L(less_1x_vec):
#  if VEC_SIZE == 64
	cmpl	$(32 / CHAR_SIZE), %edx
	jae	L(copy_32_63)
#  endif

	cmpl	$(16 / CHAR_SIZE), %edx
	jae	L(copy_16_31)


	cmpl	$(8 / CHAR_SIZE), %edx
	jae	L(copy_8_15)

#  ifdef USE_AS_WCSCPY
	vmovd	%VMM_128(0), (%rdi)
	MOVCHAR	$0, (%rdi, %rdx, CHAR_SIZE)
	ret
#  else

	cmpl	$4, %edx
	jae	L(copy_4_7)

	movzbl	(%rsi), %ecx
	cmpl	$1, %edx
	jbe	L(set_null_term)

	movzwl	1(%rsi), %esi
	movw	%si, 1(%rdi)

	.p2align 4,, 1
L(set_null_term):
	movb	%cl, (%rdi)
	MOVCHAR	$0, (%rdi, %rdx)
	ret
#  endif

#  if VEC_SIZE == 64
	.p2align 4,, 6
L(copy_32_63):
	VMOVU	-(32)(%rsi, %rdx, CHAR_SIZE), %VMM_256(1)
	VMOVU	%VMM_256(0), (%rdi)
	VMOVU	%VMM_256(1), -(32)(%rdi, %rdx, CHAR_SIZE)
	MOVCHAR	$0, (%rdi, %rdx, CHAR_SIZE)
	ret
#  endif
	.p2align 4,, 6
L(copy_16_31):
	/* Use xmm1 explicitly here as it won't require a `vzeroupper`
	   and will save code size.  */
	vmovdqu	-(16)(%rsi, %rdx, CHAR_SIZE), %xmm1
	VMOVU	%VMM_128(0), (%rdi)
	vmovdqu	%xmm1, -(16)(%rdi, %rdx, CHAR_SIZE)
	MOVCHAR	$0, (%rdi, %rdx, CHAR_SIZE)
	ret

	.p2align 4,, 2
L(copy_8_15):
	movq	-(8)(%rsi, %rdx, CHAR_SIZE), %rcx
	vmovq	%VMM_128(0), (%rdi)
	movq	%rcx, -(8)(%rdi, %rdx, CHAR_SIZE)
	MOVCHAR	$0, (%rdi, %rdx, CHAR_SIZE)
	ret

#  ifndef USE_AS_WCSCPY
	.p2align 4,, 12
L(copy_4_7):
	movl	-(4)(%rsi, %rdx, CHAR_SIZE), %ecx
	vmovd	%VMM_128(0), (%rdi)
	movl	%ecx, -(4)(%rdi, %rdx, CHAR_SIZE)
	MOVCHAR	$0, (%rdi, %rdx, CHAR_SIZE)
	ret
#  endif

# endif
	.p2align 4,, 4
L(zero_len):
# ifdef USE_AS_WCSCPY
	test	%rdx, %rdx
# endif
	jne	OVERFLOW_STRCAT
	ret

	.p2align 4,, 8
L(more_1x_vec):
	VMOVU	%VMM(0), (%rdi)

	/* We are going to align rsi here so will need to be able to re-
	   adjust rdi/rdx afterwards. NB: We filtered out huge lengths
	   so rsi + rdx * CHAR_SIZE cannot overflow.  */

	leaq	(VEC_SIZE * -1)(%rsi, %rdx, CHAR_SIZE), %rdx
	subq	%rsi, %rdi
	andq	$-(VEC_SIZE), %rsi
L(loop_last_4x_vec):
	addq	%rsi, %rdi
	subq	%rsi, %rdx
# ifdef USE_AS_WCSCPY
	shrq	$2, %rdx
# endif

	/* Will need this regardless.  */
	VMOVA	(VEC_SIZE * 1)(%rsi), %VMM(1)
	VPTESTN	%VMM(1), %VMM(1), %k0
	KMOV	%k0, %VMASK_REG

	cmpq	$(CHAR_PER_VEC * 2), %rdx
	ja	L(more_2x_vec)

L(last_2x_vec):
	FIND_FIRST_ONE (VMASK_REG, VRCX)
	cmpl	%ecx, %edx
	jbe	L(ret_vec_x1_len)

	/* If there were no zero-CHARs (rcx was zero before
	   FIND_FIRST_ONE), then ecx will be $CHAR_PER_VEC.  */
	cmpl	$CHAR_PER_VEC, %ecx
	jne	L(ret_vec_x1)

	VMOVA	(VEC_SIZE * 2)(%rsi), %VMM(2)
	VMOVU	%VMM(1), (VEC_SIZE * 1)(%rdi)
	VPTESTN	%VMM(2), %VMM(2), %k0
	KMOV	%k0, %VRCX
	addl	$-CHAR_PER_VEC, %edx
	bzhi	%VRDX, %VRCX, %VR8
	jz	L(ret_vec_x2_len)
L(ret_vec_x2):
	bsf	%VRCX, %VRDX
L(ret_vec_x2_len):
	VMOVU	(VEC_SIZE * 2 -(VEC_SIZE))(%rsi, %rdx, CHAR_SIZE), %VMM(0)
	MOVCHAR	$0, (VEC_SIZE * 2)(%rdi, %rdx, CHAR_SIZE)
	VMOVU	%VMM(0), (VEC_SIZE * 2 -(VEC_SIZE))(%rdi, %rdx, CHAR_SIZE)
	ret

	.p2align 4,, 4
L(ret_vec_x1_len):
	movl	%edx, %ecx
L(ret_vec_x1):
	VMOVU	(VEC_SIZE -(VEC_SIZE))(%rsi, %rcx, CHAR_SIZE), %VMM(0)
	MOVCHAR	$0, (VEC_SIZE)(%rdi, %rcx, CHAR_SIZE)
	VMOVU	%VMM(0), (VEC_SIZE-(VEC_SIZE))(%rdi, %rcx, CHAR_SIZE)
	VZEROUPPER_RETURN


	.p2align 4,, 8
L(last_4x_vec):
	addl	$-(CHAR_PER_VEC * 4), %edx
	VMOVA	(VEC_SIZE * 5)(%rsi), %VMM(1)
	VPTESTN	%VMM(1), %VMM(1), %k0
	KMOV	%k0, %VMASK_REG
	subq	$-(VEC_SIZE * 4), %rsi
	subq	$-(VEC_SIZE * 4), %rdi
	cmpl	$(CHAR_PER_VEC * 2), %edx
	jbe	L(last_2x_vec)
	.p2align 4,, 8
L(more_2x_vec):
# ifdef USE_AS_WCSCPY
	xorl	%ecx, %ecx
# endif
	bsf	%VMASK_REG, %VRCX
	jnz	L(ret_vec_x1)

	VMOVA	(VEC_SIZE * 2)(%rsi), %VMM(2)
	VMOVU	%VMM(1), (VEC_SIZE * 1)(%rdi)
	VPTESTN	%VMM(2), %VMM(2), %k0
	KMOV	%k0, %VRCX
	test	%VRCX, %VRCX
	jnz	L(ret_vec_x2)

	VMOVA	(VEC_SIZE * 3)(%rsi), %VMM(3)
	VMOVU	%VMM(2), (VEC_SIZE * 2)(%rdi)
	VPTESTN	%VMM(3), %VMM(3), %k0
	KMOV	%k0, %VMASK_REG

	cmpq	$(CHAR_PER_VEC * 4), %rdx
	ja	L(more_4x_vec)

	/* Adjust length before going to L(ret_vec_x3_len) or
	   L(ret_vec_x3).  */
	addl	$(CHAR_PER_VEC * -2), %edx

	FIND_FIRST_ONE (VMASK_REG, VRCX)
	cmpl	%ecx, %edx
	jbe	L(ret_vec_x3_len)

	/* If there were no zero-CHARs (rcx was zero before
	   FIND_FIRST_ONE), then ecx will be $CHAR_PER_VEC.  */
	cmpl	$CHAR_PER_VEC, %ecx
	jne	L(ret_vec_x3)

	VMOVA	(VEC_SIZE * 4)(%rsi), %VMM(4)
	VMOVU	%VMM(3), (VEC_SIZE * 3)(%rdi)
	VPTESTN	%VMM(4), %VMM(4), %k0
	KMOV	%k0, %VRCX
	addl	$-CHAR_PER_VEC, %edx
	bzhi	%VRDX, %VRCX, %VR8
	jz	L(ret_vec_x4_len)
L(ret_vec_x4):
	bsf	%VRCX, %VRDX
L(ret_vec_x4_len):
	VMOVU	(VEC_SIZE * 4 -(VEC_SIZE))(%rsi, %rdx, CHAR_SIZE), %VMM(0)
	MOVCHAR	$0, (VEC_SIZE * 4)(%rdi, %rdx, CHAR_SIZE)
	VMOVU	%VMM(0), (VEC_SIZE * 4 -(VEC_SIZE))(%rdi, %rdx, CHAR_SIZE)
	ret

	.p2align 4,, 4
L(ret_vec_x3_len):
	movl	%edx, %ecx
L(ret_vec_x3):
	VMOVU	(VEC_SIZE * 3 -(VEC_SIZE))(%rsi, %rcx, CHAR_SIZE), %VMM(0)
	MOVCHAR	$0, (VEC_SIZE * 3)(%rdi, %rcx, CHAR_SIZE)
	VMOVU	%VMM(0), (VEC_SIZE * 3-(VEC_SIZE))(%rdi, %rcx, CHAR_SIZE)
	ret

	.p2align 4,, 8
L(more_4x_vec):
# ifdef USE_AS_WCSCPY
	xorl	%ecx, %ecx
# endif
	bsf	%VMASK_REG, %VRCX
	jnz	L(ret_vec_x3)

	VMOVA	(VEC_SIZE * 4)(%rsi), %VMM(4)
	VMOVU	%VMM(3), (VEC_SIZE * 3)(%rdi)
	VPTESTN	%VMM(4), %VMM(4), %k0
	KMOV	%k0, %VRCX
	test	%VRCX, %VRCX
	jnz	L(ret_vec_x4)

	VMOVU	%VMM(4), (VEC_SIZE * 4)(%rdi)

	/* Check if we are near the end before aligning.  */
	cmpq	$(CHAR_PER_VEC * 8), %rdx
	jbe	L(last_4x_vec)


	/* Add rsi to rdx (length) before aligning rsi. NB: Since we
	   filtered out huge lengths this cannot overflow.  */
# ifdef USE_AS_WCSCPY
	leaq	(%rsi, %rdx, CHAR_SIZE), %rdx
# else
	addq	%rsi, %rdx
# endif

	/* Subtract rsi from rdi before aligning (add back will have
	   correct rdi for aligned rsi).  */
	subq	%rsi, %rdi
	subq	$-(VEC_SIZE * 5), %rsi
	andq	$(VEC_SIZE * -4), %rsi

	/* Load first half of the loop before entry.  */
	VMOVA	(VEC_SIZE * 0 + 0)(%rsi), %VMM(0)
	VMOVA	(VEC_SIZE * 1 + 0)(%rsi), %VMM(1)
	VMOVA	(VEC_SIZE * 2 + 0)(%rsi), %VMM(2)
	VMOVA	(VEC_SIZE * 3 + 0)(%rsi), %VMM(3)

	VPMIN	%VMM(0), %VMM(1), %VMM(4)
	VPMIN	%VMM(2), %VMM(3), %VMM(6)
	VPTESTN	%VMM(4), %VMM(4), %k2
	VPTESTN	%VMM(6), %VMM(6), %k4

	/* Offset rsi by VEC_SIZE so that we can jump to
	   L(loop_last_4x_vec).  */
	addq	$-(VEC_SIZE), %rsi
	KORTEST	%k2, %k4
	jnz	L(loop_4x_done)

	/* Store loop end in r9.  */
	leaq	-(VEC_SIZE * 5)(%rdx), %r9

	.p2align 4,, 11
L(loop_4x_vec):
	VMOVU	%VMM(0), (VEC_SIZE * 1 + 0)(%rdi, %rsi)
	VMOVU	%VMM(1), (VEC_SIZE * 2 + 0)(%rdi, %rsi)
	VMOVU	%VMM(2), (VEC_SIZE * 3 + 0)(%rdi, %rsi)
	VMOVU	%VMM(3), (VEC_SIZE * 4 + 0)(%rdi, %rsi)

	subq	$(VEC_SIZE * -4), %rsi
	cmpq	%rsi, %r9
	jbe	L(loop_last_4x_vec)

	VMOVA	(VEC_SIZE * 1 + 0)(%rsi), %VMM(0)
	VMOVA	(VEC_SIZE * 2 + 0)(%rsi), %VMM(1)
	VMOVA	(VEC_SIZE * 3 + 0)(%rsi), %VMM(2)
	VMOVA	(VEC_SIZE * 4 + 0)(%rsi), %VMM(3)

	VPMIN	%VMM(0), %VMM(1), %VMM(4)
	VPMIN	%VMM(2), %VMM(3), %VMM(6)
	VPTESTN	%VMM(4), %VMM(4), %k2
	VPTESTN	%VMM(6), %VMM(6), %k4
	KORTEST	%k2, %k4
	jz	L(loop_4x_vec)

L(loop_4x_done):
	VPTESTN	%VMM(0), %VMM(0), %k0
	KMOV	%k0, %VRCX
	/* Restore rdi (dst).  */
	addq	%rsi, %rdi

	/* L(ret_vec_x1) expects rcx to have position of zero-CHAR so
	   test with bsf.  */
	bsf	%VRCX, %VRCX
	jnz	L(ret_vec_x1)
	VMOVU	%VMM(0), (VEC_SIZE * 1 + 0)(%rdi)

	KMOV	%k2, %VRCX
	test	%VRCX, %VRCX
	jnz	L(ret_vec_x2)
	VMOVU	%VMM(1), (VEC_SIZE * 2 + 0)(%rdi)

	VPTESTN	%VMM(2), %VMM(2), %k0
	KMOV	%k0, %VRCX
	bsf	%VRCX, %VRCX
	jnz	L(ret_vec_x3)
	VMOVU	%VMM(2), (VEC_SIZE * 3 + 0)(%rdi)

	KMOV	%k4, %VRCX
	bsf	%VRCX, %VRCX
	VMOVU	((VEC_SIZE * 4)-(VEC_SIZE - CHAR_SIZE))(%rsi, %rcx, CHAR_SIZE), %VMM(0)
	VMOVU	%VMM(0), ((VEC_SIZE * 4 + 0)-(VEC_SIZE - CHAR_SIZE))(%rdi, %rcx, CHAR_SIZE)
	ret


	.p2align 4,, 4
L(page_cross):
	movq	%rsi, %r8
	andq	$(VEC_SIZE * -1), %r8
	VPCMPEQ	(%r8), %VZERO, %k0

# ifdef USE_AS_WCSCPY
	KMOV	%k0, %VR9
	shrl	$2, %ecx
	andl	$(CHAR_PER_VEC - 1), %ecx
	shrx	%VRCX, %VR9, %VRCX
# else
	KMOV	%k0, %VRCX
	shrx	%VRSI, %VRCX, %VRCX
# endif

	subl	%esi, %r8d
	andl	$(VEC_SIZE - 1), %r8d
# ifdef USE_AS_WCSCPY
	shrl	$2, %r8d
# endif
	cmpq	%r8, %rdx
	jbe	L(page_cross_small)
	/* Optimizing more for space as this is very cold code. This
	   saves 2x cache lines.  */

	/* This adds once to the later result which will get correct
	   copy bounds. NB: this can never zero-out a non-zero RCX as
	   to be in the page cross case rsi cannot be aligned and we
	   already right-shift rcx by the misalignment.  */
	shl	%VRCX
	jz	L(page_cross_continue)
	bsf	%VRCX, %VRCX
	REP_MOVS
	ret

L(page_cross_small):
	tzcnt	%VRCX, %VRCX
	jz	L(page_cross_setz)
	cmpl	%edx, %ecx
	cmova	%edx, %ecx

# ifdef USE_AS_WCSCPY
	rep	movsd
# else
	rep	movsb
# endif
L(page_cross_setz):
	MOVCHAR	$0, (%rdi)
	ret
END(STRNCAT)
#endif
