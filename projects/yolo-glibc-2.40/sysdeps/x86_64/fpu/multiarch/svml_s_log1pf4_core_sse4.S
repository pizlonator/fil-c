/* Function log1pf vectorized with SSE4.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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
   https://www.gnu.org/licenses/.  */

/*
 * ALGORITHM DESCRIPTION:
 *
 *    1+x = 2^k*(xh + xl) is computed in high-low parts; xh in [1, 2)
 *    Get short reciprocal approximation Rcp ~ 1/xh
 *    R = (Rcp*xh - 1.0) + Rcp*xl
 *    log1p(x) = k*log(2.0) - log(Rcp) + poly(R)
 *       log(Rcp) is tabulated
 *
 *
 */

/* Offsets for data table __svml_slog1p_data_internal
 */
#define SgnMask				0
#define sOne				16
#define sPoly				32
#define iHiDelta			160
#define iLoRange			176
#define iBrkValue			192
#define iOffExpoMask			208
#define sLn2				224

#include <sysdep.h>

	.section .text.sse4, "ax", @progbits
ENTRY(_ZGVbN4v_log1pf_sse4)
	subq	$72, %rsp
	cfi_def_cfa_offset(80)
	movups	sOne+__svml_slog1p_data_internal(%rip), %xmm7

	/* compute 1+x as high, low parts */
	movaps	%xmm7, %xmm1
	movaps	%xmm7, %xmm5
	maxps	%xmm0, %xmm1
	minps	%xmm0, %xmm5
	movaps	%xmm1, %xmm4

	/* check argument value ranges */
	movdqu	iHiDelta+__svml_slog1p_data_internal(%rip), %xmm2
	addps	%xmm5, %xmm4

	/* reduction: compute r, n */
	movdqu	iBrkValue+__svml_slog1p_data_internal(%rip), %xmm3
	paddd	%xmm4, %xmm2
	movdqu	iOffExpoMask+__svml_slog1p_data_internal(%rip), %xmm8
	subps	%xmm4, %xmm1
	psubd	%xmm3, %xmm4
	addps	%xmm1, %xmm5
	pand	%xmm4, %xmm8
	psrad	$23, %xmm4
	cvtdq2ps %xmm4, %xmm10
	pslld	$23, %xmm4
	movaps	%xmm7, %xmm1
	paddd	%xmm3, %xmm8
	psubd	%xmm4, %xmm1
	mulps	%xmm5, %xmm1

	/* polynomial evaluation */
	subps	%xmm7, %xmm8

	/* final reconstruction */
	mulps	sLn2+__svml_slog1p_data_internal(%rip), %xmm10
	addps	%xmm8, %xmm1
	movups	sPoly+112+__svml_slog1p_data_internal(%rip), %xmm9
	mulps	%xmm1, %xmm9
	movdqu	iLoRange+__svml_slog1p_data_internal(%rip), %xmm6
	pcmpgtd	%xmm2, %xmm6
	addps	sPoly+96+__svml_slog1p_data_internal(%rip), %xmm9

	/* combine and get argument value range mask */
	movmskps %xmm6, %edx
	movups	SgnMask+__svml_slog1p_data_internal(%rip), %xmm11
	mulps	%xmm1, %xmm9
	andnps	%xmm0, %xmm11
	addps	sPoly+80+__svml_slog1p_data_internal(%rip), %xmm9
	mulps	%xmm1, %xmm9
	addps	sPoly+64+__svml_slog1p_data_internal(%rip), %xmm9
	mulps	%xmm1, %xmm9
	addps	sPoly+48+__svml_slog1p_data_internal(%rip), %xmm9
	mulps	%xmm1, %xmm9
	addps	sPoly+32+__svml_slog1p_data_internal(%rip), %xmm9
	mulps	%xmm1, %xmm9
	addps	sPoly+16+__svml_slog1p_data_internal(%rip), %xmm9
	mulps	%xmm1, %xmm9
	addps	sPoly+__svml_slog1p_data_internal(%rip), %xmm9
	mulps	%xmm1, %xmm9
	mulps	%xmm1, %xmm9
	addps	%xmm9, %xmm1
	addps	%xmm10, %xmm1
	orps	%xmm11, %xmm1
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx rbp r12 r13 r14 r15 edx xmm0 xmm1

	/* Restore registers
	 * and exit the function
	 */

L(EXIT):
	movaps	%xmm1, %xmm0
	addq	$72, %rsp
	cfi_def_cfa_offset(8)
	ret
	cfi_def_cfa_offset(80)

	/* Branch to process
	 * special inputs
	 */

L(SPECIAL_VALUES_BRANCH):
	movups	%xmm0, 32(%rsp)
	movups	%xmm1, 48(%rsp)
	# LOE rbx rbp r12 r13 r14 r15 edx

	xorl	%eax, %eax
	movq	%r12, 16(%rsp)
	cfi_offset(12, -64)
	movl	%eax, %r12d
	movq	%r13, 8(%rsp)
	cfi_offset(13, -72)
	movl	%edx, %r13d
	movq	%r14, (%rsp)
	cfi_offset(14, -80)
	# LOE rbx rbp r15 r12d r13d

	/* Range mask
	 * bits check
	 */

L(RANGEMASK_CHECK):
	btl	%r12d, %r13d

	/* Call scalar math function */
	jc	L(SCALAR_MATH_CALL)
	# LOE rbx rbp r15 r12d r13d

	/* Special inputs
	 * processing loop
	 */

L(SPECIAL_VALUES_LOOP):
	incl	%r12d
	cmpl	$4, %r12d

	/* Check bits in range mask */
	jl	L(RANGEMASK_CHECK)
	# LOE rbx rbp r15 r12d r13d

	movq	16(%rsp), %r12
	cfi_restore(12)
	movq	8(%rsp), %r13
	cfi_restore(13)
	movq	(%rsp), %r14
	cfi_restore(14)
	movups	48(%rsp), %xmm1

	/* Go to exit */
	jmp	L(EXIT)
	cfi_offset(12, -64)
	cfi_offset(13, -72)
	cfi_offset(14, -80)
	# LOE rbx rbp r12 r13 r14 r15 xmm1

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	movss	32(%rsp, %r14, 4), %xmm0
	call	log1pf@PLT
	# LOE rbx rbp r14 r15 r12d r13d xmm0

	movss	%xmm0, 48(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx rbp r15 r12d r13d
END(_ZGVbN4v_log1pf_sse4)

	.section .rodata, "a"
	.align	16

#ifdef __svml_slog1p_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(16)) VUINT32 SgnMask[4][1];
	__declspec(align(16)) VUINT32 sOne[4][1];
	__declspec(align(16)) VUINT32 sPoly[8][4][1];
	__declspec(align(16)) VUINT32 iHiDelta[4][1];
	__declspec(align(16)) VUINT32 iLoRange[4][1];
	__declspec(align(16)) VUINT32 iBrkValue[4][1];
	__declspec(align(16)) VUINT32 iOffExpoMask[4][1];
	__declspec(align(16)) VUINT32 sLn2[4][1];
} __svml_slog1p_data_internal;
#endif
__svml_slog1p_data_internal:
	/* SgnMask */
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff
	/* sOne = SP 1.0 */
	.align	16
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
	/* sPoly[] = SP polynomial */
	.align	16
	.long	0xbf000000, 0xbf000000, 0xbf000000, 0xbf000000 /* -5.0000000000000000000000000e-01 P0 */
	.long	0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94, 0x3eaaaa94 /* 3.3333265781402587890625000e-01 P1 */
	.long	0xbe80058e, 0xbe80058e, 0xbe80058e, 0xbe80058e /* -2.5004237890243530273437500e-01 P2 */
	.long	0x3e4ce190, 0x3e4ce190, 0x3e4ce190, 0x3e4ce190 /* 2.0007920265197753906250000e-01 P3 */
	.long	0xbe28ad37, 0xbe28ad37, 0xbe28ad37, 0xbe28ad37 /* -1.6472326219081878662109375e-01 P4 */
	.long	0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12, 0x3e0fcb12 /* 1.4042308926582336425781250e-01 P5 */
	.long	0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3, 0xbe1ad9e3 /* -1.5122179687023162841796875e-01 P6 */
	.long	0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed, 0x3e0d84ed /* 1.3820238411426544189453125e-01 P7 */
	/* iHiDelta = SP 80000000-7f000000 */
	.align	16
	.long	0x01000000, 0x01000000, 0x01000000, 0x01000000
	/* iLoRange = SP 00800000+iHiDelta */
	.align	16
	.long	0x01800000, 0x01800000, 0x01800000, 0x01800000
	/* iBrkValue = SP 2/3 */
	.align	16
	.long	0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab
	/* iOffExpoMask = SP significand mask */
	.align	16
	.long	0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff
	/* sLn2 = SP ln(2) */
	.align	16
	.long	0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218
	.align	16
	.type	__svml_slog1p_data_internal, @object
	.size	__svml_slog1p_data_internal, .-__svml_slog1p_data_internal
