/* Function log10f vectorized with SSE4.
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
 *    Get short reciprocal approximation Rcp ~ 1/mantissa(x)
 *    R = Rcp*x - 1.0
 *    log10(x) = k*log10(2.0) - log10(Rcp) + poly_approximation(R)
 *       log10(Rcp) is tabulated
 *
 *
 */

/* Offsets for data table __svml_slog10_data_internal
 */
#define MinNorm				0
#define MaxNorm				16
#define L2H				32
#define L2L				48
#define iBrkValue			64
#define iOffExpoMask			80
#define One				96
#define sPoly				112
#define L2				256

#include <sysdep.h>

	.section .text.sse4, "ax", @progbits
ENTRY(_ZGVbN4v_log10f_sse4)
	subq	$72, %rsp
	cfi_def_cfa_offset(80)
	movaps	%xmm0, %xmm1

	/* reduction: compute r, n */
	movdqu	iBrkValue+__svml_slog10_data_internal(%rip), %xmm2
	movaps	%xmm0, %xmm4
	movdqu	iOffExpoMask+__svml_slog10_data_internal(%rip), %xmm10
	psubd	%xmm2, %xmm1
	pand	%xmm1, %xmm10
	psrad	$23, %xmm1
	paddd	%xmm2, %xmm10
	movaps	%xmm0, %xmm3
	movups	sPoly+__svml_slog10_data_internal(%rip), %xmm5
	movups	sPoly+32+__svml_slog10_data_internal(%rip), %xmm6
	movups	sPoly+64+__svml_slog10_data_internal(%rip), %xmm7
	movups	sPoly+96+__svml_slog10_data_internal(%rip), %xmm9
	cvtdq2ps %xmm1, %xmm12
	cmpltps	MinNorm+__svml_slog10_data_internal(%rip), %xmm4
	cmpnleps MaxNorm+__svml_slog10_data_internal(%rip), %xmm3
	subps	One+__svml_slog10_data_internal(%rip), %xmm10
	mulps	%xmm10, %xmm5
	movaps	%xmm10, %xmm8
	mulps	%xmm10, %xmm6
	mulps	%xmm10, %xmm8
	addps	sPoly+16+__svml_slog10_data_internal(%rip), %xmm5
	mulps	%xmm10, %xmm7
	addps	sPoly+48+__svml_slog10_data_internal(%rip), %xmm6
	mulps	%xmm10, %xmm9
	mulps	%xmm8, %xmm5
	addps	sPoly+80+__svml_slog10_data_internal(%rip), %xmm7
	addps	sPoly+112+__svml_slog10_data_internal(%rip), %xmm9
	addps	%xmm5, %xmm6
	mulps	%xmm8, %xmm6
	orps	%xmm3, %xmm4

	/* combine and get argument value range mask */
	movmskps %xmm4, %edx
	movups	L2L+__svml_slog10_data_internal(%rip), %xmm1
	addps	%xmm6, %xmm7
	mulps	%xmm12, %xmm1
	mulps	%xmm7, %xmm8
	movups	L2H+__svml_slog10_data_internal(%rip), %xmm11
	addps	%xmm8, %xmm9
	mulps	%xmm11, %xmm12
	mulps	%xmm10, %xmm9
	addps	sPoly+128+__svml_slog10_data_internal(%rip), %xmm9
	mulps	%xmm9, %xmm10
	addps	%xmm10, %xmm1
	addps	%xmm12, %xmm1
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
	call	log10f@PLT
	# LOE rbx rbp r14 r15 r12d r13d xmm0

	movss	%xmm0, 48(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx rbp r15 r12d r13d
END(_ZGVbN4v_log10f_sse4)

	.section .rodata, "a"
	.align	16

#ifdef __svml_slog10_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(16)) VUINT32 MinNorm[4][1];
	__declspec(align(16)) VUINT32 MaxNorm[4][1];
	__declspec(align(16)) VUINT32 L2H[4][1];
	__declspec(align(16)) VUINT32 L2L[4][1];
	__declspec(align(16)) VUINT32 iBrkValue[4][1];
	__declspec(align(16)) VUINT32 iOffExpoMask[4][1];
	__declspec(align(16)) VUINT32 One[4][1];
	__declspec(align(16)) VUINT32 sPoly[9][4][1];
	__declspec(align(16)) VUINT32 L2[4][1];
} __svml_slog10_data_internal;
#endif
__svml_slog10_data_internal:
	/* MinNorm */
	.long	0x00800000, 0x00800000, 0x00800000, 0x00800000
	/* MaxNorm */
	.align	16
	.long	0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff
	/* L2H */
	.align	16
	.long	0x3e9a2100, 0x3e9a2100, 0x3e9a2100, 0x3e9a2100
	/* L2L */
	.align	16
	.long	0xb64AF600, 0xb64AF600, 0xb64AF600, 0xb64AF600
	/* iBrkValue = SP 2/3 */
	.align	16
	.long	0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab
	/* iOffExpoMask = SP significand mask */
	.align	16
	.long	0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff
	/* sOne = SP 1.0 */
	.align	16
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
	/* spoly[9] */
	.align	16
	.long	0x3d8063B4, 0x3d8063B4, 0x3d8063B4, 0x3d8063B4 /* coeff9 */
	.long	0xbd890073, 0xbd890073, 0xbd890073, 0xbd890073 /* coeff8 */
	.long	0x3d775317, 0x3d775317, 0x3d775317, 0x3d775317 /* coeff7 */
	.long	0xbd91FB27, 0xbd91FB27, 0xbd91FB27, 0xbd91FB27 /* coeff6 */
	.long	0x3dB20B96, 0x3dB20B96, 0x3dB20B96, 0x3dB20B96 /* coeff5 */
	.long	0xbdDE6E20, 0xbdDE6E20, 0xbdDE6E20, 0xbdDE6E20 /* coeff4 */
	.long	0x3e143CE5, 0x3e143CE5, 0x3e143CE5, 0x3e143CE5 /* coeff3 */
	.long	0xbe5E5BC5, 0xbe5E5BC5, 0xbe5E5BC5, 0xbe5E5BC5 /* coeff2 */
	.long	0x3eDE5BD9, 0x3eDE5BD9, 0x3eDE5BD9, 0x3eDE5BD9 /* coeff1 */
	/* L2 */
	.align	16
	.long	0x3e9a209b, 0x3e9a209b, 0x3e9a209b, 0x3e9a209b
	.align	16
	.type	__svml_slog10_data_internal, @object
	.size	__svml_slog10_data_internal, .-__svml_slog10_data_internal
