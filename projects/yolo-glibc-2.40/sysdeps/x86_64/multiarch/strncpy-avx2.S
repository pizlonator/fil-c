/* strncpy with AVX2
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

# ifndef STRNCPY
#  define STRNCPY	__strncpy_avx2
# endif


# ifdef USE_AS_WCSCPY
#  define VPCMPEQ	vpcmpeqd
#  define VPMIN	vpminud
#  define CHAR_SIZE	4
# else
#  define VPCMPEQ	vpcmpeqb
#  define VPMIN	vpminub
#  define CHAR_SIZE	1
# endif

# include "strncpy-or-cat-overflow-def.h"

# define PAGE_SIZE	4096

# define VZERO	VMM(7)
# define VZERO_128	VMM_128(7)


	.section SECTION(.text), "ax", @progbits
ENTRY(STRNCPY)
# ifdef __ILP32__
	/* Clear the upper 32 bits.  */
	movl	%edx, %edx
# endif
	/* Filter zero length strings and very long strings.  Zero
	   length strings just return, very long strings are handled by
	   just running rep stos{b|l} to zero set (which will almost
	   certainly segfault), if that succeeds then just calling
	   OVERFLOW_STRCPY (strcpy, stpcpy, wcscpy, wcpcpy).  */
# ifdef USE_AS_WCSCPY
	decq	%rdx
	movq	%rdx, %rax
	/* 56 is end of max supported address space.  */
	shr	$56, %rax
	jnz	L(zero_len)
	salq	$2, %rdx
# else
	decq	%rdx
	/* `dec` can macrofuse with `jl`. If the flag needs to become
	   `jb` replace `dec` with `sub`.  */
	jl	L(zero_len)
# endif

	vpxor	%VZERO_128, %VZERO_128, %VZERO_128
	movl	%esi, %eax
	andl	$(PAGE_SIZE - 1), %eax
	cmpl	$(PAGE_SIZE - VEC_SIZE), %eax
	ja	L(page_cross)

L(page_cross_continue):
	VMOVU	(%rsi), %VMM(0)
	VPCMPEQ	%VMM(0), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %ecx

	/* If no STPCPY just save end ahead of time.  */
# ifndef USE_AS_STPCPY
	movq	%rdi, %rax
# elif defined USE_AS_WCSCPY
	/* Clear dependency as nearly all return code for wcpncpy uses
	   `setc %al`.  */
	xorl	%eax, %eax
# endif

	cmpq	$(VEC_SIZE - CHAR_SIZE), %rdx
	/* `jb` because length rdx is now length - CHAR_SIZE.  */
	jbe	L(less_1x_vec)

	/* This may overset but that's fine because we still need to zero
	   fill.  */
	VMOVU	%VMM(0), (%rdi)

	testl	%ecx, %ecx
	jnz	L(zfill)

	/* Align.  */
	addq	%rsi, %rdx
	subq	%rsi, %rdi
	orq	$(VEC_SIZE - 1), %rsi
	incq	%rsi
L(last_4x_vec):
	addq	%rsi, %rdi
L(loop_last_4x_vec):
	subq	%rsi, %rdx


	VMOVA	0(%rsi), %VMM(1)
	VPCMPEQ	%VMM(1), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %ecx

	cmpq	$(VEC_SIZE * 2), %rdx
	jae	L(more_2x_vec)

	cmpl	$(VEC_SIZE), %edx
	jb	L(ret_vec_x1_len)

	testl	%ecx, %ecx
	jnz	L(ret_vec_x1)

	VPCMPEQ	VEC_SIZE(%rsi), %VZERO, %VMM(6)
	VMOVU	%VMM(1), (%rdi)
	vpmovmskb %VMM(6), %ecx
	shlq	$VEC_SIZE, %rcx
L(ret_vec_x1_len):
	tzcntq	%rcx, %rcx
	cmpl	%ecx, %edx
	jbe	L(ret_vec_x1_len_no_zfill)
	/* Fall through (expectation) is copy len < buffer len.  */
	VMOVU	%VZERO, ((0)-(VEC_SIZE - CHAR_SIZE))(%rdi, %rdx)
L(ret_vec_x1_len_no_zfill_mov):
	movl	%ecx, %edx
# ifdef USE_AS_STPCPY
	/* clear flags.  */
	xorl	%ecx, %ecx
# endif
L(ret_vec_x1_len_no_zfill):
	VMOVU	((0)-(VEC_SIZE - CHAR_SIZE))(%rsi, %rdx), %VMM(1)
	VMOVU	%VMM(1), ((0)-(VEC_SIZE - CHAR_SIZE))(%rdi, %rdx)
# ifdef USE_AS_STPCPY
#  ifdef USE_AS_WCSCPY
	setc	%al
	addq	%rdx, %rdi
	leaq	(%rdi, %rax, CHAR_SIZE), %rax
#  else
	movl	%edx, %eax
	adcq	%rdi, %rax
#  endif
# endif
L(return_vzeroupper):
	ZERO_UPPER_VEC_REGISTERS_RETURN

	.p2align 4,, 6
L(ret_vec_x1):
	bsfl	%ecx, %ecx
	VMOVU	%VZERO, ((0)-(VEC_SIZE - CHAR_SIZE))(%rdi, %rdx)
	subl	%ecx, %edx
	/* Check if we need to reload/store.  */
	cmpl	$VEC_SIZE, %edx
	jb	L(ret_vec_x1_len_no_zfill_mov)
	/* Otherwise safe to just store directly.  */
	VMOVU	%VMM(1), (%rdi)
	VMOVU	%VZERO, (%rdi, %rcx)
# ifdef USE_AS_STPCPY
	leaq	(%rdi, %rcx), %rax
# endif
	VZEROUPPER_RETURN

	.p2align 4,, 12
L(more_2x_vec):
	VMOVU	%VMM(1), (%rdi)
	testl	%ecx, %ecx
	/* Must fill at least 2x VEC.  */
	jnz	L(zfill_vec1)

	VMOVA	VEC_SIZE(%rsi), %VMM(2)
	VMOVU	%VMM(2), VEC_SIZE(%rdi)
	VPCMPEQ	%VMM(2), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %ecx
	testl	%ecx, %ecx
	/* Must fill at least 1x VEC.  */
	jnz	L(zfill_vec2)

	VMOVA	(VEC_SIZE * 2)(%rsi), %VMM(3)
	VPCMPEQ	%VMM(3), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %ecx

	/* Check if len is more 4x VEC. -CHAR_SIZE because rdx is len -
	   CHAR_SIZE.  */
	cmpq	$(VEC_SIZE * 4 - CHAR_SIZE), %rdx
	ja	L(more_4x_vec)

	subl	$(VEC_SIZE * 3), %edx
	jb	L(ret_vec_x3_len)

	testl	%ecx, %ecx
	jnz	L(ret_vec_x3)

	VPCMPEQ	(VEC_SIZE * 3)(%rsi), %VZERO, %VMM(6)
	VMOVU	%VMM(3), (VEC_SIZE * 2)(%rdi)
	vpmovmskb %VMM(6), %ecx
	tzcntl	%ecx, %ecx
	cmpl	%ecx, %edx
	jbe	L(ret_vec_x4_len_no_zfill)
	/* Fall through (expectation) is copy len < buffer len.  */
	VMOVU	%VZERO, ((VEC_SIZE * 3)-(VEC_SIZE - CHAR_SIZE))(%rdi, %rdx)
	movl	%ecx, %edx
L(ret_vec_x4_len_no_zfill):
	VMOVU	((VEC_SIZE * 3)-(VEC_SIZE - CHAR_SIZE))(%rsi, %rdx), %VMM(1)
	VMOVU	%VMM(1), ((VEC_SIZE * 3)-(VEC_SIZE - CHAR_SIZE))(%rdi, %rdx)
# ifdef USE_AS_STPCPY
#  ifdef USE_AS_WCSCPY
	setc	%al
	addq	%rdx, %rdi
	leaq	(VEC_SIZE * 3)(%rdi, %rax, CHAR_SIZE), %rax
#  else
	leal	(VEC_SIZE * 3 + 0)(%edx), %eax
	adcq	%rdi, %rax
#  endif
# endif
	VZEROUPPER_RETURN


L(ret_vec_x3_len):
	addl	$(VEC_SIZE * 1), %edx
	tzcntl	%ecx, %ecx
	cmpl	%ecx, %edx
	jbe	L(ret_vec_x3_len_no_zfill)
	/* Fall through (expectation) is copy len < buffer len.  */
	VMOVU	%VZERO, ((VEC_SIZE * 2)-(VEC_SIZE - CHAR_SIZE))(%rdi, %rdx)
L(ret_vec_x3_len_no_zfill_mov):
	movl	%ecx, %edx
# ifdef USE_AS_STPCPY
	/* clear flags.  */
	xorl	%ecx, %ecx
# endif
	.p2align 4,, 4
L(ret_vec_x3_len_no_zfill):
	VMOVU	((VEC_SIZE * 2)-(VEC_SIZE - CHAR_SIZE))(%rsi, %rdx), %VMM(1)
	VMOVU	%VMM(1), ((VEC_SIZE * 2)-(VEC_SIZE - CHAR_SIZE))(%rdi, %rdx)
# ifdef USE_AS_STPCPY
#  ifdef USE_AS_WCSCPY
	setc	%al
	addq	%rdx, %rdi
	leaq	(VEC_SIZE * 2)(%rdi, %rax, CHAR_SIZE), %rax
#  else
	leal	(VEC_SIZE * 2 + 0)(%rdx), %eax
	adcq	%rdi, %rax
#  endif
# endif
	VZEROUPPER_RETURN


	.p2align 4,, 8
L(ret_vec_x3):
	bsfl	%ecx, %ecx
	VMOVU	%VZERO, (VEC_SIZE * 3 +(-(VEC_SIZE - CHAR_SIZE)))(%rdi, %rdx)
	subl	%ecx, %edx
	jl	L(ret_vec_x3_len_no_zfill_mov)
	VMOVU	%VMM(3), (VEC_SIZE * 2)(%rdi)
	VMOVU	%VZERO, (VEC_SIZE * 2)(%rdi, %rcx)
# ifdef USE_AS_STPCPY
	leaq	(VEC_SIZE * 2)(%rdi, %rcx), %rax
# endif
	VZEROUPPER_RETURN

	.p2align 4,, 8
L(more_4x_vec):

	VMOVU	%VMM(3), (VEC_SIZE * 2)(%rdi)
	testl	%ecx, %ecx
	jnz	L(zfill_vec3)

	VMOVA	(VEC_SIZE * 3)(%rsi), %VMM(4)
	VMOVU	%VMM(4), (VEC_SIZE * 3)(%rdi)
	VPCMPEQ	%VMM(4), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %ecx
	testl	%ecx, %ecx
	jnz	L(zfill_vec4)

	movq	%rdx, %rcx
	addq	%rsi, %rdx
	subq	%rsi, %rdi
	subq	$-(VEC_SIZE * 4), %rsi
	/* Recheck length before aligning.  */
	cmpq	$(VEC_SIZE * 8 - CHAR_SIZE), %rcx
	jbe	L(last_4x_vec)

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

	/* Use r9 as end register.  */
	leaq	-(VEC_SIZE * 4 - CHAR_SIZE)(%rdx), %r9

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
	subq	%rsi, %rdx
	VMOVU	%VMM(0), (VEC_SIZE * 0 + 0)(%rdi)
	VPCMPEQ	%VMM(0), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %ecx
	testl	%ecx, %ecx
	jnz	L(zfill_vec1)

	VMOVU	%VMM(1), (VEC_SIZE * 1 + 0)(%rdi)
	VPCMPEQ	%VMM(1), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %ecx
	testl	%ecx, %ecx
	jnz	L(zfill_vec2)

	VMOVU	%VMM(2), (VEC_SIZE * 2 + 0)(%rdi)
	VPCMPEQ	%VMM(2), %VZERO, %VMM(6)
	vpmovmskb %VMM(6), %ecx
	testl	%ecx, %ecx
	jnz	L(zfill_vec3)

	VMOVU	%VMM(3), (VEC_SIZE * 3 + 0)(%rdi)
	movl	%r8d, %ecx

	// Zfill more....

	.p2align 4,, 4
L(zfill_vec4):
	addq	$(VEC_SIZE * 2), %rdi
	subq	$(VEC_SIZE * 2), %rdx
L(zfill_vec2):
	shlq	$VEC_SIZE, %rcx
L(zfill):
	bsfq	%rcx, %rcx
	subq	%rcx, %rdx
	addq	%rcx, %rdi
# ifdef USE_AS_STPCPY
	movq	%rdi, %rax
# endif
L(zfill_from_page_cross):
	cmpq	$VEC_SIZE, %rdx
	jb	L(zfill_less_vec_vzeroupper)

L(zfill_more_1x_vec):
	VMOVU	%VZERO, CHAR_SIZE(%rdi)
	VMOVU	%VZERO, (CHAR_SIZE - VEC_SIZE)(%rdi, %rdx)
	cmpq	$(VEC_SIZE * 2), %rdx
	jae	L(zfill_more_2x_vec)
L(zfill_done0):
	VZEROUPPER_RETURN

	.p2align 4,, 8
L(zfill_vec3):
	addq	$(VEC_SIZE * 2), %rdi
	subq	$(VEC_SIZE * 2), %rdx
	.p2align 4,, 2
L(zfill_vec1):
	bsfl	%ecx, %ecx
	addq	%rcx, %rdi
	subq	%rcx, %rdx
# ifdef USE_AS_STPCPY
	movq	%rdi, %rax
# endif
	/* zfill from vec1/vec3 must have to set at least 2x VECS.  */

	VMOVU	%VZERO, CHAR_SIZE(%rdi)
	VMOVU	%VZERO, (CHAR_SIZE - VEC_SIZE)(%rdi, %rdx)
	cmpq	$(VEC_SIZE * 2), %rdx
	jb	L(zfill_done0)
L(zfill_more_2x_vec):
	VMOVU	%VZERO, (CHAR_SIZE - VEC_SIZE * 2)(%rdi, %rdx)
	VMOVU	%VZERO, (VEC_SIZE + CHAR_SIZE)(%rdi)
	subq	$(VEC_SIZE * 4 - CHAR_SIZE), %rdx
	jbe	L(zfill_done)

	addq	%rdi, %rdx
	VMOVU	%VZERO, (VEC_SIZE * 2 + CHAR_SIZE)(%rdi)
	VMOVU	%VZERO, (VEC_SIZE * 3 + CHAR_SIZE)(%rdi)


	VMOVU	%VZERO, (VEC_SIZE * 0 + 0)(%rdx)
	VMOVU	%VZERO, (VEC_SIZE * 1 + 0)(%rdx)

	subq	$-(VEC_SIZE * 4 + CHAR_SIZE), %rdi
	cmpq	%rdi, %rdx
	jbe	L(zfill_done)

	andq	$-(VEC_SIZE), %rdi
	.p2align 4,, 12
L(zfill_loop_4x_vec):
	VMOVA	%VZERO, (VEC_SIZE * 0)(%rdi)
	VMOVA	%VZERO, (VEC_SIZE * 1)(%rdi)
	VMOVA	%VZERO, (VEC_SIZE * 2)(%rdi)
	VMOVA	%VZERO, (VEC_SIZE * 3)(%rdi)
	subq	$-(VEC_SIZE * 4), %rdi
	cmpq	%rdi, %rdx
	ja	L(zfill_loop_4x_vec)
L(zfill_done):
	VZEROUPPER_RETURN


	.p2align 4,, 8
L(copy_1x):
	VMOVU	%VMM(0), (%rdi)
	testl	%ecx, %ecx
	jz	L(ret_32_32)
L(zfill_less_vec):
	bsfl	%ecx, %ecx
L(zfill_less_vec_no_bsf):
	subq	%rcx, %rdx
	addq	%rcx, %rdi
# ifdef USE_AS_STPCPY
	movq	%rdi, %rax
# endif
L(zfill_less_vec_vzeroupper):
	COND_VZEROUPPER
	/* We are taking advantage of the fact that to be here we must
	   be writing null-term as (%rdi, %rcx) we have a byte of lee-
	   way for overwriting.  */
	cmpl	$16, %edx
	jb	L(zfill_less_16)
	VMOVU	%VZERO_128, (%rdi)
	VMOVU	%VZERO_128, -(16 - CHAR_SIZE)(%rdi, %rdx)
	ret
# ifdef USE_AS_STPCPY
L(ret_32_32):
	leaq	CHAR_SIZE(%rdi, %rdx), %rax
	VZEROUPPER_RETURN
# endif

	.p2align 4,, 4
L(copy_16_31):
	/* Overfill to avoid branches.  */
	vmovdqu	-(16 - CHAR_SIZE)(%rsi, %rdx), %xmm1
	vmovdqu	%xmm0, (%rdi)
	vmovdqu	%xmm1, -(16 - CHAR_SIZE)(%rdi, %rdx)
	cmpl	%ecx, %edx
	ja	L(zfill_less_vec_no_bsf)
# ifndef USE_AS_STPCPY
L(ret_32_32):
# else
#  ifdef USE_AS_WCSCPY
	setc	%al
	addq	%rdx, %rdi
	leaq	(%rdi, %rax, CHAR_SIZE), %rax
#  else
	movl	%edx, %eax
	adcq	%rdi, %rax
#  endif
# endif
	VZEROUPPER_RETURN

	.p2align 4,, 4
L(copy_8_15):
	/* Overfill to avoid branches.  */
	movq	-(8 - CHAR_SIZE)(%rsi, %rdx), %rsi
	vmovq	%xmm0, (%rdi)
	movq	%rsi, -(8 - CHAR_SIZE)(%rdi, %rdx)
	cmpl	%ecx, %edx
	jbe	L(ret_8_15)
	subq	%rcx, %rdx
	addq	%rcx, %rdi
# ifdef USE_AS_STPCPY
	movq	%rdi, %rax
# endif
	.p2align 4,, 8
L(zfill_less_16):
	xorl	%ecx, %ecx
	cmpl	$8, %edx
	jb	L(zfill_less_8)
	movq	%rcx, (%rdi)
	movq	%rcx, -(8 - CHAR_SIZE)(%rdi, %rdx)
# ifndef USE_AS_STPCPY
L(ret_8_15):
# endif
	ret


	.p2align 4,, 8
L(less_1x_vec):
	/* Reuse flag from `cmp $VEC_SIZE, %rdx`. The idea is many
	   buffer sizes are aligned conventially.  */
	je	L(copy_1x)

	tzcntl	%ecx, %ecx
	cmpl	$16, %edx
	jae	L(copy_16_31)

	COND_VZEROUPPER
	cmpl	$8, %edx
	jae	L(copy_8_15)
# ifdef USE_AS_WCSCPY
	testl	%ecx, %ecx
	jz	L(zfill_less_8_set_ret)

	movl	(%rsi, %rdx), %esi
	vmovd	%xmm0, (%rdi)
	movl	%esi, (%rdi, %rdx)

#  ifdef USE_AS_STPCPY
	cmpl	%ecx, %edx
L(ret_8_15):
	setc	%al
	addq	%rdx, %rdi
	leaq	(%rdi, %rax, CHAR_SIZE), %rax
#  endif
	ret
L(zfill_less_8_set_ret):
	xorl	%ecx, %ecx
#  ifdef USE_AS_STPCPY
	movq	%rdi, %rax
#  endif
L(zfill_less_8):
	movl	%ecx, (%rdi)
	movl	%ecx, (%rdi, %rdx)
	ret

# else
	cmpl	$3, %edx
	jb	L(copy_0_3)
	/* Overfill to avoid branches.  */
	movl	-3(%rsi, %rdx), %esi
	vmovd	%xmm0, (%rdi)
	movl	%esi, -3(%rdi, %rdx)
	cmpl	%ecx, %edx
	jbe	L(ret_4_7)
	subq	%rcx, %rdx
	addq	%rcx, %rdi
#  ifdef USE_AS_STPCPY
	movq	%rdi, %rax
#  endif
	xorl	%ecx, %ecx
	.p2align 4,, 8
L(zfill_less_8):
	cmpl	$3, %edx
	jb	L(zfill_less_3)
	movl	%ecx, (%rdi)
	movl	%ecx, -3(%rdi, %rdx)
#  ifdef USE_AS_STPCPY
	ret
#  endif

L(ret_4_7):
#  ifdef USE_AS_STPCPY
L(ret_8_15):
	movl	%edx, %eax
	adcq	%rdi, %rax
#  endif
	ret

	.p2align 4,, 4
L(zfill_less_3):
	testl	%edx, %edx
	jz	L(zfill_1)
	movw	%cx, (%rdi)
L(zfill_1):
	movb	%cl, (%rdi, %rdx)
	ret

	.p2align 4,, 8
L(copy_0_3):
	vmovd	%xmm0, %r8d
	testl	%edx, %edx
	jz	L(copy_1)
	movw	%r8w, (%rdi)
	cmpl	%ecx, %edx
	ja	L(zfill_from_1)
	movzbl	(%rsi, %rdx), %r8d
#  ifdef USE_AS_STPCPY
	movl	%edx, %eax
	adcq	%rdi, %rax
	movb	%r8b, (%rdi, %rdx)
	ret
#  endif

L(copy_1):
#  ifdef USE_AS_STPCPY
	movl	%edx, %eax
	cmpl	%ecx, %edx
	adcq	%rdi, %rax
#  endif
#  ifdef USE_AS_WCSCPY
	vmovd	%xmm0, (%rdi)
#  else
	movb	%r8b, (%rdi, %rdx)
#  endif
	ret
# endif

	.p2align 4,, 2
L(zero_len):
	movq	%rdi, %rax
	ret
# ifndef USE_AS_WCSCPY
	.p2align 4,, 8
L(zfill_from_1):
#  ifdef USE_AS_STPCPY
	leaq	(%rdi, %rcx), %rax
#  endif
	movw	$0, -1(%rdi, %rdx)
	ret
# endif

	.p2align 4,, 4
	.p2align 6,, 8
L(page_cross):
	movq	%rsi, %rax
	andq	$(VEC_SIZE * -1), %rax

	VPCMPEQ	(%rax), %VZERO, %VMM(6)

	vpmovmskb %VMM(6), %ecx
	shrxl	%esi, %ecx, %ecx

	subl	%esi, %eax
	andl	$(VEC_SIZE - 1), %eax
	cmpq	%rax, %rdx
	jb	L(page_cross_small)
	/* Optimizing more aggressively for space as this is very cold
	   code. This saves 2x cache lines.  */

	/* If rcx is non-zero then continue.  */
	shl	$CHAR_SIZE, %ecx
	jz	L(page_cross_continue)
	bsf	%ecx, %ecx

	subq	%rcx, %rdx
# ifdef USE_AS_STPCPY
	leaq	-CHAR_SIZE(%rdi, %rcx), %rax
# else
	movq	%rdi, %rax
# endif

	rep	movsb
# ifdef USE_AS_WCSCPY
	movl	$0, (%rdi)
# else
	movb	$0, (%rdi)
# endif
	jmp	L(zfill_from_page_cross)

L(page_cross_small):
	tzcntl	%ecx, %ecx
	xorl	%eax, %eax
	cmpl	%ecx, %edx
	jbe	L(page_cross_copy_only)

	/* Do a zfill of the tail before copying.  */
	movq	%rdi, %r9
	movl	%ecx, %r8d

	subl	%ecx, %edx
	leaq	CHAR_SIZE(%rdi, %rcx), %rdi
	movl	%edx, %ecx
	rep	stosb
	movq	%r9, %rdi
	movl	%r8d, %edx
L(page_cross_copy_only):
	leal	CHAR_SIZE(%rdx), %ecx
# ifdef USE_AS_STPCPY
#  ifdef USE_AS_WCSCPY
	setc	%al
	addq	%rdi, %rdx
	leaq	(%rdx, %rax, CHAR_SIZE), %rax
#  else
	movl	%edx, %eax
	adcq	%rdi, %rax
#  endif
# else
	movq	%rdi, %rax
# endif
	rep	movsb
	ret


L(best_effort_strncpy):
	movq	%rdx, %rcx
	xorl	%eax, %eax
	movq	%rdi, %r8
	/* The length is >= 2^63. We very much so expect to segfault at
	   rep stos. If that doesn't happen then just strcpy to finish.
	 */
# ifdef USE_AS_WCSCPY
	rep	stosl
# else
	rep	stosb
# endif
	movq	%r8, %rdi
	jmp	OVERFLOW_STRCPY
END(STRNCPY)
#endif
