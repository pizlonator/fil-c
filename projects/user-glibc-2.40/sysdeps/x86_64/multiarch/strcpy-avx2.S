/* strcpy with AVX2
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

#include <isa-level.h>

#if ISA_SHOULD_BUILD (3)

# include <sysdep.h>

# ifndef VEC_SIZE
#  include "x86-avx-vecs.h"
# endif

# ifndef STRCPY
#  define STRCPY	__strcpy_avx2
# endif

	/* Use movsb in page cross case to save code size.  */
# define USE_MOVSB_IN_PAGE_CROSS	1

# ifdef USE_AS_WCSCPY
#  define VPCMPEQ	vpcmpeqd
#  define VPMIN	vpminud
#  define CHAR_SIZE	4
# else
#  define VPCMPEQ	vpcmpeqb
#  define VPMIN	vpminub
#  define CHAR_SIZE	1
# endif

# define PAGE_SIZE	4096

# ifdef USE_AS_STPCPY
#  define END_REG	rax
# else
#  define END_REG	rdi, %rdx
# endif

# ifdef USE_AS_STRCAT
#  define PAGE_ALIGN_REG	ecx
# else
#  define PAGE_ALIGN_REG	eax
# endif

# define VZERO	VMM(7)
# define VZERO_128	VMM_128(7)

	.section SECTION(.text), "ax", @progbits
ENTRY(STRCPY)
	vpxor	%VZERO_128, %VZERO_128, %VZERO_128

# ifdef USE_AS_STRCAT
	movq	%rdi, %rax
#  include "strcat-strlen-avx2.h.S"
# endif

	movl	%esi, %PAGE_ALIGN_REG
	andl	$(PAGE_SIZE - 1), %PAGE_ALIGN_REG
	cmpl	$(PAGE_SIZE - VEC_SIZE), %PAGE_ALIGN_REG
	ja	L(page_cross)
L(page_cross_continue):
# if !defined USE_AS_STPCPY && !defined USE_AS_STRCAT
	movq	%rdi, %rax
# endif
	VMOVU	(%rsi), %VMM(0)
	VPCMPEQ	%VMM(0), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %ecx

	testl	%ecx, %ecx
	jz	L(more_1x_vec)

	/* No longer need ymm registers so just vzeroupper so it doesn't
	   need to be duplicated at each return statement.  */
	COND_VZEROUPPER

	xorl	%edx, %edx
	bsfl	%ecx, %edx
# ifdef USE_AS_STPCPY
	leaq	(%rdi, %rdx), %rax
# endif

	/* Use mask bits in rcx to detect which copy we need. If the low
	   mask is zero then there must be a bit set in the upper half.
	   I.e if ecx != 0 and cx == 0, then match must be upper 16
	   bits so we use L(copy_16_31).  */
	testw	%cx, %cx
	jz	L(copy_16_31)

	testb	%cl, %cl
	jz	L(copy_8_15)
# ifdef USE_AS_WCSCPY
	vmovd	%xmm0, (%rdi)
	movl	$0, (%END_REG)
	ret
# else
	testb	$0x7, %cl
	jz	L(copy_4_7)

	testl	%edx, %edx
	jz	L(set_null_term)
	vmovd	%xmm0, %ecx
	movw	%cx, (%rdi)

	.p2align 4,, 2
L(set_null_term):
	movb	$0, (%END_REG)
	ret

	.p2align 4,, 12
L(copy_4_7):
	movl	-3(%rsi, %rdx), %ecx
	vmovd	%xmm0, (%rdi)
	movl	%ecx, -3(%END_REG)
	ret
# endif

	.p2align 4,, 10
L(copy_16_31):
	VMOVU	-(16 - CHAR_SIZE)(%rsi, %rdx), %xmm1
	VMOVU	%xmm0, (%rdi)
	VMOVU	%xmm1, -(16 - CHAR_SIZE)(%END_REG)
	ret

	.p2align 4,, 10
L(copy_8_15):
# ifdef USE_AS_WCSCPY
	movl	-(8 - CHAR_SIZE)(%rsi, %rdx), %ecx
# else
	movq	-(8 - CHAR_SIZE)(%rsi, %rdx), %rcx
# endif
	vmovq	%xmm0, (%rdi)
	movq	%rcx, -(8 - CHAR_SIZE)(%END_REG)
	ret


	.p2align 4,, 8
L(more_1x_vec):
# if defined USE_AS_STPCPY || defined USE_AS_STRCAT
	VMOVU	%VMM(0), (%rdi)
# endif
	subq	%rsi, %rdi
	orq	$(VEC_SIZE - 1), %rsi
	addq	%rsi, %rdi
	VMOVA	1(%rsi), %VMM(1)

	/* Try and order stores after as many loads as is reasonable to
	   avoid potential false dependencies.  */
# if !defined USE_AS_STPCPY && !defined USE_AS_STRCAT
	VMOVU	%VMM(0), (%rax)
# endif
	VPCMPEQ	%VMM(1), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %ecx
	testl	%ecx, %ecx
	jnz	L(ret_vec_x1)

	VMOVA	(VEC_SIZE + 1)(%rsi), %VMM(2)
	VMOVU	%VMM(1), 1(%rdi)

	VPCMPEQ	%VMM(2), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %ecx
	testl	%ecx, %ecx
	jnz	L(ret_vec_x2)

	VMOVA	(VEC_SIZE * 2 + 1)(%rsi), %VMM(3)
	VMOVU	%VMM(2), (VEC_SIZE + 1)(%rdi)

	VPCMPEQ	%VMM(3), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %ecx
	testl	%ecx, %ecx
	jnz	L(ret_vec_x3)

	VMOVA	(VEC_SIZE * 3 + 1)(%rsi), %VMM(4)
	VMOVU	%VMM(3), (VEC_SIZE * 2 + 1)(%rdi)
	VPCMPEQ	%VMM(4), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %edx
	testl	%edx, %edx
	jnz	L(ret_vec_x4)

	VMOVU	%VMM(4), (VEC_SIZE * 3 + 1)(%rdi)

	/* Subtract rsi from rdi before aligning. Adding back rsi will
	   get proper rdi (dst) for new src.  */
	subq	%rsi, %rdi
	incq	%rsi
	orq	$(VEC_SIZE * 4 - 1), %rsi

	/* Do first half of loop ahead of time so loop can just start by
	   storing.  */
	VMOVA	(VEC_SIZE * 0 + 1)(%rsi), %VMM(0)
	VMOVA	(VEC_SIZE * 1 + 1)(%rsi), %VMM(1)
	VMOVA	(VEC_SIZE * 2 + 1)(%rsi), %VMM(2)
	VMOVA	(VEC_SIZE * 3 + 1)(%rsi), %VMM(3)

	VPMIN	%VMM(0), %VMM(1), %VMM(4)
	VPMIN	%VMM(2), %VMM(3), %VMM(6)
	VPMIN	%VMM(4), %VMM(6), %VMM(6)
	VPCMPEQ	%VMM(6), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %edx
	addq	%rsi, %rdi

	testl	%edx, %edx
	jnz	L(loop_4x_done)

	.p2align 4,, 11
L(loop_4x_vec):

	VMOVU	%VMM(0), (VEC_SIZE * 0 + 1)(%rdi)
	VMOVU	%VMM(1), (VEC_SIZE * 1 + 1)(%rdi)
	subq	$(VEC_SIZE * -4), %rsi
	VMOVU	%VMM(2), (VEC_SIZE * 2 + 1)(%rdi)
	VMOVU	%VMM(3), (VEC_SIZE * 3 + 1)(%rdi)


	VMOVA	(VEC_SIZE * 0 + 1)(%rsi), %VMM(0)
	VMOVA	(VEC_SIZE * 1 + 1)(%rsi), %VMM(1)
	VMOVA	(VEC_SIZE * 2 + 1)(%rsi), %VMM(2)
	VMOVA	(VEC_SIZE * 3 + 1)(%rsi), %VMM(3)

	VPMIN	%VMM(0), %VMM(1), %VMM(4)
	VPMIN	%VMM(2), %VMM(3), %VMM(6)
	VPMIN	%VMM(4), %VMM(6), %VMM(6)
	VPCMPEQ	%VMM(6), %VZERO, %VMM(6)

	vpmovmskb %VMM(6), %edx
	subq	$(VEC_SIZE * -4), %rdi
	testl	%edx, %edx
	jz	L(loop_4x_vec)

L(loop_4x_done):
	VPCMPEQ	%VMM(0), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %ecx
	testl	%ecx, %ecx
	jnz	L(ret_vec_x1)
	VMOVU	%VMM(0), (VEC_SIZE * 0 + 1)(%rdi)

	VPCMPEQ	%VMM(1), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %ecx
	testl	%ecx, %ecx
	jnz	L(ret_vec_x2)
	VMOVU	%VMM(1), (VEC_SIZE * 1 + 1)(%rdi)

	VPCMPEQ	%VMM(2), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %ecx
	testl	%ecx, %ecx
	jnz	L(ret_vec_x3)
	VMOVU	%VMM(2), (VEC_SIZE * 2 + 1)(%rdi)
L(ret_vec_x4):
	bsfl	%edx, %edx
	VMOVU	((VEC_SIZE * 3 + 1)-(VEC_SIZE - CHAR_SIZE))(%rsi, %rdx), %VMM(1)
	VMOVU	%VMM(1), ((VEC_SIZE * 3 + 1)-(VEC_SIZE - CHAR_SIZE))(%rdi, %rdx)
# ifdef USE_AS_STPCPY
	leaq	(VEC_SIZE * 3 + 1)(%rdx, %rdi), %rax
# endif
L(return_end):
	VZEROUPPER_RETURN

	.p2align 4,, 8
L(ret_vec_x1):
	bsfl	%ecx, %ecx
	VMOVU	(1 -(VEC_SIZE - CHAR_SIZE))(%rsi, %rcx), %VMM(1)
	VMOVU	%VMM(1), (1 -(VEC_SIZE - CHAR_SIZE))(%rdi, %rcx)
# ifdef USE_AS_STPCPY
	leaq	1(%rcx, %rdi), %rax
# endif
L(return_vzeroupper):
	ZERO_UPPER_VEC_REGISTERS_RETURN

	.p2align 4,, 8
L(ret_vec_x2):
	bsfl	%ecx, %ecx
	VMOVU	((VEC_SIZE + 1)-(VEC_SIZE - CHAR_SIZE))(%rsi, %rcx), %VMM(1)
	VMOVU	%VMM(1), ((VEC_SIZE + 1)-(VEC_SIZE - CHAR_SIZE))(%rdi, %rcx)
# ifdef USE_AS_STPCPY
	leaq	(VEC_SIZE * 1 + 1)(%rcx, %rdi), %rax
# endif
	VZEROUPPER_RETURN

	.p2align 4,, 8
L(ret_vec_x3):
	bsfl	%ecx, %ecx
	VMOVU	((VEC_SIZE * 2 + 1)-(VEC_SIZE - CHAR_SIZE))(%rsi, %rcx), %VMM(1)
	VMOVU	%VMM(1), ((VEC_SIZE * 2 + 1)-(VEC_SIZE - CHAR_SIZE))(%rdi, %rcx)
# ifdef USE_AS_STPCPY
	leaq	(VEC_SIZE * 2 + 1)(%rcx, %rdi), %rax
# endif
	VZEROUPPER_RETURN


	.p2align 4,, 4
L(page_cross):
	movq	%rsi, %rcx
	andq	$(VEC_SIZE * -1), %rcx

	VPCMPEQ	(%rcx), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %ecx
	shrxl	%esi, %ecx, %ecx
# if USE_MOVSB_IN_PAGE_CROSS
	/* Optimizing more aggressively for space as this is very cold
	   code. This saves 2x cache lines.  */

	/* This adds once to the later result which will get correct
	   copy bounds. NB: this can never zero-out a non-zero RCX as
	   to be in the page cross case rsi cannot be aligned and we
	   already right-shift rcx by the misalignment.  */
	shll	$CHAR_SIZE, %ecx
	jz	L(page_cross_continue)
	bsfl	%ecx, %ecx
#  if !defined USE_AS_STPCPY && !defined USE_AS_STRCAT
	movq	%rdi, %rax
#  endif
	rep	movsb
#  ifdef USE_AS_STPCPY
	leaq	-CHAR_SIZE(%rdi), %rax
#  endif

	VZEROUPPER_RETURN

# else
	testl	%ecx, %ecx
	jz	L(page_cross_continue)

	/* Traditional copy case, essentially same as used in non-page-
	   cross case but since we can't reuse VMM(0) we need twice as
	   many loads from rsi.  */
#  ifndef USE_AS_STRCAT
	xorl	%edx, %edx
#  endif
	bsfl	%ecx, %edx
#  ifdef USE_AS_STPCPY
	leaq	(%rdi, %rdx), %rax
#  elif !defined USE_AS_STRCAT
	movq	%rdi, %rax
#  endif

	/* vzeroupper early to avoid duplicating at each return.  */
	COND_VZEROUPPER

	testw	%cx, %cx
	jz	L(page_cross_copy_16_31)

	testb	%cl, %cl
	jz	L(page_cross_copy_8_15)

	testl	$0x7, %cl
	jz	L(page_cross_copy_4_7)

	testl	%edx, %edx
	jz	L(page_cross_set_null_term)
	movzwl	(%rsi), %ecx
	movw	%cx, (%rdi)
L(page_cross_set_null_term):
	movb	$0, (%END_REG)
	ret

	.p2align 4,, 4
L(page_cross_copy_4_7):
	movl	(%rsi), %ecx
	movl	-3(%rsi, %rdx), %esi
	movl	%ecx, (%rdi)
	movl	%esi, -3(%END_REG)
	ret

	.p2align 4,, 4
L(page_cross_copy_8_15):
	movq	(%rsi), %rcx
	movq	-7(%rsi, %rdx), %rsi
	movq	%rcx, (%rdi)
	movq	%rsi, -7(%END_REG)
	ret


	.p2align 4,, 3
L(page_cross_copy_16_31):
	VMOVU	(%rsi), %xmm0
	VMOVU	-15(%rsi, %rdx), %xmm1
	VMOVU	%xmm0, (%rdi)
	VMOVU	%xmm1, -15(%END_REG)
	ret
# endif

END(STRCPY)
#endif
