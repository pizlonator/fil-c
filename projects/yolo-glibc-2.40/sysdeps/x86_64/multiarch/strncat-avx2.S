/* strncat with AVX2
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

#if ISA_SHOULD_BUILD (3)

# include <sysdep.h>

# ifndef VEC_SIZE
#  include "x86-avx-vecs.h"
# endif

# ifndef STRNCAT
#  define STRNCAT	__strncat_avx2
# endif

# ifdef USE_AS_WCSCPY
#  define MOVCHAR	movl
#  define VPCMPEQ	vpcmpeqd
#  define VPMIN	vpminud
#  define CHAR_SIZE	4
# else
#  define MOVCHAR	movb
#  define VPCMPEQ	vpcmpeqb
#  define VPMIN	vpminub
#  define CHAR_SIZE	1
# endif

# include "strncpy-or-cat-overflow-def.h"

# define PAGE_SIZE	4096

# define VZERO	VMM(7)
# define VZERO_128	VMM_128(7)

	.section SECTION(.text), "ax", @progbits
ENTRY(STRNCAT)
# ifdef __ILP32__
	/* Clear the upper 32 bits.  */
	movl	%edx, %edx
# endif
	/* Filter zero length strings and very long strings.  Zero
	   length strings just return, very long strings are handled by
	   using the non-length variant {wcs|str}cat.  */
	movq	%rdi, %rax
# ifdef USE_AS_WCSCPY
	leaq	-1(%rdx), %rcx
	shr	$56, %rcx
	jnz	L(zero_len)
	salq	$2, %rdx
# else
	test	%rdx, %rdx
	jle	L(zero_len)
# endif
	vpxor	%VZERO_128, %VZERO_128, %VZERO_128

# include "strcat-strlen-avx2.h.S"

	movl	%esi, %ecx
	andl	$(PAGE_SIZE - 1), %ecx
	cmpl	$(PAGE_SIZE - VEC_SIZE), %ecx
	ja	L(page_cross)
L(page_cross_continue):
	VMOVU	(%rsi), %VMM(0)
	VPCMPEQ	%VMM(0), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %ecx

	tzcnt	%ecx, %r8d
	cmpq	%r8, %rdx
	jbe	L(less_1x_vec)

	testl	%ecx, %ecx
	jz	L(more_1x_vec)

	/* Hoist this to save code size.  */

	movl	%r8d, %edx

L(less_1x_vec):
	COND_VZEROUPPER

	cmpl	$16, %edx
	jae	L(copy_16_31)
	cmpl	$8, %edx
	jae	L(copy_8_15)


# ifdef USE_AS_WCSCPY
	vmovd	%VMM_128(0), (%rdi)
	MOVCHAR	$0, (%rdi, %rdx)
	ret
# else
	cmpl	$4, %edx
	jae	L(copy_4_7)

	movzbl	(%rsi), %ecx
	cmpl	$1, %edx
	jbe	L(set_null_term)

	/* NB: make this `vmovw` if support for AVX512-FP16 is added.
	 */
	movzwl	1(%rsi), %esi
	movw	%si, 1(%rdi)

	.p2align 4,, 1
L(set_null_term):
	movb	%cl, (%rdi)
	MOVCHAR	$0, (%rdi, %rdx)
	ret

	.p2align 4,, 11
L(copy_4_7):
	movl	-(4)(%rsi, %rdx), %ecx
	vmovd	%xmm0, (%rdi)
	movl	%ecx, -(4)(%rdi, %rdx)
	MOVCHAR	$0, (%rdi, %rdx)
	ret
# endif


	.p2align 4,, 10
L(copy_16_31):
	VMOVU	-(16)(%rsi, %rdx), %xmm1
	VMOVU	%xmm0, (%rdi)
	VMOVU	%xmm1, -(16)(%rdi, %rdx)
	MOVCHAR	$0, (%rdi, %rdx)
	ret

	.p2align 4,, 10
L(copy_8_15):
	movq	-(8)(%rsi, %rdx), %rcx
	vmovq	%xmm0, (%rdi)
	movq	%rcx, -(8)(%rdi, %rdx)
	MOVCHAR	$0, (%rdi, %rdx)
	ret

	.p2align 4,, 8
	.p2align 6,, 14
L(more_1x_vec):
	VMOVU	%VMM(0), (%rdi)

	/* Align rsi (src) and just rdx/rdi (length/dst).  */
	addq	%rsi, %rdx
	subq	%rsi, %rdi
	orq	$(VEC_SIZE - 1), %rsi
	incq	%rsi
	addq	%rsi, %rdi
L(loop_last_4x_vec):
	subq	%rsi, %rdx
	VMOVA	0(%rsi), %VMM(1)
	VPCMPEQ	%VMM(1), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %ecx
	cmpq	$(VEC_SIZE * 2), %rdx
	ja	L(more_2x_vec)
L(last_2x_vec):
	tzcnt	%ecx, %ecx
	cmpl	%ecx, %edx
	jbe	L(ret_vec_x1_len)

	cmpl	$VEC_SIZE, %ecx
	jnz	L(ret_vec_x1)

	VMOVA	(VEC_SIZE * 1)(%rsi), %VMM(2)
	VMOVU	%VMM(1), (%rdi)
	VPCMPEQ	%VMM(2), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %ecx
	addl	$-VEC_SIZE, %edx
	bzhil	%edx, %ecx, %r8d
	jz	L(ret_vec_x2_len)
L(ret_vec_x2):
	bsfl	%ecx, %edx
L(ret_vec_x2_len):
	VMOVU	(%rsi, %rdx), %VMM(0)
	MOVCHAR	$0, (VEC_SIZE)(%rdi, %rdx)
	VMOVU	%VMM(0), (%rdi, %rdx)
L(return_vzeroupper):
	ZERO_UPPER_VEC_REGISTERS_RETURN


	.p2align 4,, 12
L(ret_vec_x1_len):
	movl	%edx, %ecx
L(ret_vec_x1):
	VMOVU	-(VEC_SIZE)(%rsi, %rcx), %VMM(1)
	MOVCHAR	$0, (%rdi, %rcx)
	VMOVU	%VMM(1), -VEC_SIZE(%rdi, %rcx)
	VZEROUPPER_RETURN

	.p2align 4,, 8
L(last_4x_vec):
	subq	$-(VEC_SIZE * 4), %rsi
	VMOVA	0(%rsi), %VMM(1)
	VPCMPEQ	%VMM(1), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %ecx
	subq	$-(VEC_SIZE * 4), %rdi
	addl	$-(VEC_SIZE * 4), %edx
	cmpl	$(VEC_SIZE * 2), %edx
	jbe	L(last_2x_vec)
	.p2align 4,, 8
L(more_2x_vec):
	/* L(ret_vec_x1) expects ecx to have position of first match so
	   test with bsf.  */
	bsfl	%ecx, %ecx
	jnz	L(ret_vec_x1)

	VMOVA	(VEC_SIZE * 1)(%rsi), %VMM(2)
	VMOVU	%VMM(1), (%rdi)

	VPCMPEQ	%VMM(2), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %ecx
	testl	%ecx, %ecx
	jnz	L(ret_vec_x2)


	VMOVA	(VEC_SIZE * 2)(%rsi), %VMM(3)
	VMOVU	%VMM(2), (VEC_SIZE * 1)(%rdi)

	VPCMPEQ	%VMM(3), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %ecx

	/* Check if length is greater than 4x VEC.  */
	cmpq	$(VEC_SIZE * 4), %rdx
	ja	L(more_4x_vec)

	addl	$(VEC_SIZE * -2), %edx

	tzcnt	%ecx, %ecx
	cmpl	%ecx, %edx
	jbe	L(ret_vec_x3_len)

	cmpl	$VEC_SIZE, %ecx
	jnz	L(ret_vec_x3)

	VMOVA	(VEC_SIZE * 3 + 0)(%rsi), %VMM(4)
	VMOVU	%VMM(3), (VEC_SIZE * 2 + 0)(%rdi)
	VPCMPEQ	%VMM(4), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %ecx
	addl	$-VEC_SIZE, %edx
	bzhil	%edx, %ecx, %r8d
	jz	L(ret_vec_x4_len)
L(ret_vec_x4):
	bsfl	%ecx, %edx
L(ret_vec_x4_len):
	VMOVU	(VEC_SIZE * 2)(%rsi, %rdx), %VMM(0)
	MOVCHAR	$0, (VEC_SIZE * 3)(%rdi, %rdx)
	VMOVU	%VMM(0), (VEC_SIZE * 2)(%rdi, %rdx)
	VZEROUPPER_RETURN

	.p2align 4,, 4
L(ret_vec_x3_len):
	movl	%edx, %ecx
L(ret_vec_x3):
	VMOVU	(VEC_SIZE)(%rsi, %rcx), %VMM(0)
	MOVCHAR	$0, (VEC_SIZE * 2)(%rdi, %rcx)
	VMOVU	%VMM(0), (VEC_SIZE)(%rdi, %rcx)
	VZEROUPPER_RETURN


	.p2align 4,, 8
L(more_4x_vec):
	bsfl	%ecx, %ecx
	jnz	L(ret_vec_x3)

	VMOVA	(VEC_SIZE * 3)(%rsi), %VMM(4)
	VMOVU	%VMM(3), (VEC_SIZE * 2)(%rdi)
	VPCMPEQ	%VMM(4), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %ecx
	testl	%ecx, %ecx
	jnz	L(ret_vec_x4)

	VMOVU	%VMM(4), (VEC_SIZE * 3)(%rdi)


	/* Recheck length before aligning.  */
	cmpq	$(VEC_SIZE * 8), %rdx
	jbe	L(last_4x_vec)

	/* Align rsi (src) and just rdx/rdi (length/dst).  */
	addq	%rsi, %rdx
	subq	%rsi, %rdi
	subq	$-(VEC_SIZE * 4), %rsi
	andq	$(VEC_SIZE * -4), %rsi

	/* Do first half of loop ahead of time so loop can just start by
	   storing.  */
	VMOVA	(VEC_SIZE * 0 + 0)(%rsi), %VMM(0)
	VMOVA	(VEC_SIZE * 1 + 0)(%rsi), %VMM(1)
	VMOVA	(VEC_SIZE * 2 + 0)(%rsi), %VMM(2)
	VMOVA	(VEC_SIZE * 3 + 0)(%rsi), %VMM(3)

	VPMIN	%VMM(0), %VMM(1), %VMM(4)
	VPMIN	%VMM(2), %VMM(3), %VMM(6)
	VPMIN	%VMM(4), %VMM(6), %VMM(6)
	VPCMPEQ	%VMM(6), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %r8d
	addq	%rsi, %rdi
	testl	%r8d, %r8d
	jnz	L(loop_4x_done)

	/* Use r9 for end of region before handling last 4x VEC
	   specially.  */
	leaq	-(VEC_SIZE * 4)(%rdx), %r9

	.p2align 4,, 11
L(loop_4x_vec):

	VMOVU	%VMM(0), (VEC_SIZE * 0 + 0)(%rdi)
	VMOVU	%VMM(1), (VEC_SIZE * 1 + 0)(%rdi)
	subq	$(VEC_SIZE * -4), %rsi
	VMOVU	%VMM(2), (VEC_SIZE * 2 + 0)(%rdi)
	VMOVU	%VMM(3), (VEC_SIZE * 3 + 0)(%rdi)

	subq	$(VEC_SIZE * -4), %rdi
	cmpq	%rsi, %r9
	jbe	L(loop_last_4x_vec)

	VMOVA	(VEC_SIZE * 0 + 0)(%rsi), %VMM(0)
	VMOVA	(VEC_SIZE * 1 + 0)(%rsi), %VMM(1)
	VMOVA	(VEC_SIZE * 2 + 0)(%rsi), %VMM(2)
	VMOVA	(VEC_SIZE * 3 + 0)(%rsi), %VMM(3)

	VPMIN	%VMM(0), %VMM(1), %VMM(4)
	VPMIN	%VMM(2), %VMM(3), %VMM(6)
	VPMIN	%VMM(4), %VMM(6), %VMM(6)
	VPCMPEQ	%VMM(6), %VZERO, %VMM(6)

	vpmovmskb %VMM(6), %r8d

	testl	%r8d, %r8d
	jz	L(loop_4x_vec)

L(loop_4x_done):
	VPCMPEQ	%VMM(0), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %ecx
	/* L(ret_vec_x1) expects ecx to have position of first match so
	   test with bsf.  */
	bsfl	%ecx, %ecx
	jnz	L(ret_vec_x1)
	VMOVU	%VMM(0), (VEC_SIZE * 0 + 0)(%rdi)

	VPCMPEQ	%VMM(1), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %ecx

	testl	%ecx, %ecx
	jnz	L(ret_vec_x2)
	VMOVU	%VMM(1), (VEC_SIZE * 1 + 0)(%rdi)

	VPCMPEQ	%VMM(2), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %ecx
	bsfl	%ecx, %ecx
	jnz	L(ret_vec_x3)

	VMOVU	%VMM(2), (VEC_SIZE * 2 + 0)(%rdi)
	bsfl	%r8d, %r8d
	VMOVU	(VEC_SIZE * 2 + CHAR_SIZE)(%rsi, %r8), %VMM(1)
	VMOVU	%VMM(1), (VEC_SIZE * 2 + CHAR_SIZE)(%rdi, %r8)
	VZEROUPPER_RETURN



	.p2align 4,, 4
L(page_cross):
	movq	%rsi, %r8
	andq	$(VEC_SIZE * -1), %r8

	VPCMPEQ	(%r8), %VZERO, %VMM(6)

	vpmovmskb %VMM(6), %ecx
	shrxl	%esi, %ecx, %ecx

	subl	%esi, %r8d
	andl	$(VEC_SIZE - 1), %r8d
	cmpq	%r8, %rdx
	jbe	L(page_cross_small)

	/* Optimizing more aggressively for space as this is very cold
	   code. This saves 2x cache lines.  */

	/* This adds once to the later result which will get correct
	   copy bounds. NB: this can never zero-out a non-zero RCX as
	   to be in the page cross case rsi cannot be aligned and we
	   already right-shift rcx by the misalignment.  */
	shll	$CHAR_SIZE, %ecx
	jz	L(page_cross_continue)
	bsfl	%ecx, %ecx
	rep	movsb
	VZEROUPPER_RETURN

L(page_cross_small):
	tzcntl	%ecx, %ecx
	jz	L(page_cross_setz)
	cmpl	%edx, %ecx
	cmova	%edx, %ecx
	rep	movsb
L(page_cross_setz):
	MOVCHAR	$0, (%rdi)
	VZEROUPPER_RETURN
L(zero_len):
# ifdef USE_AS_WCSCPY
	test	%rdx, %rdx
# endif
	jnz	OVERFLOW_STRCAT
	ret


END(STRNCAT)
#endif
