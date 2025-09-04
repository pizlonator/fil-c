/* Function acoshf vectorized with SSE4.
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
 *   Compute acosh(x) as log(x + sqrt(x*x - 1))
 *
 *   Special cases:
 *
 *   acosh(NaN)  = quiet NaN, and raise invalid exception
 *   acosh(-INF) = NaN
 *   acosh(+INF) = +INF
 *   acosh(x)    = NaN if x < 1
 *   acosh(1)    = +0
 *
 */

/* Offsets for data table __svml_sacosh_data_internal
 */
#define sOne				0
#define sPoly				16
#define iBrkValue			144
#define iOffExpoMask			160
#define sBigThreshold			176
#define sC2				192
#define sC3				208
#define sHalf				224
#define sLargestFinite			240
#define sThirtyOne			256
#define sTopMask8			272
#define XScale				288
#define sLn2				304

#include <sysdep.h>

	.section .text.sse4, "ax", @progbits
ENTRY(_ZGVbN4v_acoshf_sse4)
	subq	$72, %rsp
	cfi_def_cfa_offset(80)

	/* Compute U = X - 1 and V = X + 1, naively first. */
	movaps	%xmm0, %xmm12

	/* Load constants, always including One = 1 */
	movups	sOne+__svml_sacosh_data_internal(%rip), %xmm2

	/*
	 * Check that 1 < X < +inf; otherwise go to the callout function.
	 * We need the callout for X = 1 to avoid division by zero below.
	 * This test ensures that callout handles NaN and either infinity.
	 */
	movaps	%xmm0, %xmm4
	movaps	%xmm2, %xmm9

	/*
	 * Compute e = -(2 * d + d^2)
	 * The first FMR is exact, and the rounding error in the other is acceptable
	 * since d and e are ~ 2^-8
	 */
	movaps	%xmm2, %xmm10

	/* Finally, express Y + W = U * V accurately where Y has <= 8 bits */
	movups	sTopMask8+__svml_sacosh_data_internal(%rip), %xmm5

	/*
	 * Now we feed into the log1p code, using H in place of _VARG1 and
	 * also adding L into Xl.
	 * compute 1+x as high, low parts
	 */
	movaps	%xmm2, %xmm13
	movaps	%xmm5, %xmm11
	movaps	%xmm2, %xmm3

	/*
	 * Now       1 / (1 + d)
	 * = 1 / (1 + (sqrt(1 - e) - 1))
	 * = 1 / sqrt(1 - e)
	 * = 1 + 1/2 * e + 3/8 * e^2 + 5/16 * e^3 + 35/128 * e^4 + ...
	 * So compute the first three nonconstant terms of that, so that
	 * we have a relative correction (1 + Corr) to apply to S etc.
	 * C1 = 1/2
	 * C2 = 3/8
	 * C3 = 5/16
	 */
	movups	sC3+__svml_sacosh_data_internal(%rip), %xmm8

	/*
	 * The following computation can go wrong for very large X, e.g.
	 * the X^2 - 1 = U * V can overflow. But for large X we have
	 * acosh(X) / log(2 X) - 1 =~= 1/(4 * X^2), so for X >= 2^30
	 * we can just later stick X back into the log and tweak up the exponent.
	 * Actually we scale X by 2^-30 and tweak the exponent up by 31,
	 * to stay in the safe range for the later log computation.
	 * Compute a flag now telling us when to do this.
	 */
	movaps	%xmm0, %xmm1
	cmpnleps sLargestFinite+__svml_sacosh_data_internal(%rip), %xmm4
	cmpltps	sBigThreshold+__svml_sacosh_data_internal(%rip), %xmm1
	cmpnltps %xmm0, %xmm3
	subps	%xmm2, %xmm12
	addps	%xmm0, %xmm9

	/* For low-accuracy versions, naivety is harmless */
	mulps	%xmm12, %xmm9
	orps	%xmm3, %xmm4
	movmskps %xmm4, %edx
	andps	%xmm9, %xmm11
	movaps	%xmm1, %xmm3

	/*
	 * Compute R = 1/sqrt(Y + W) * (1 + d)
	 * Force R to <= 8 significant bits.
	 * This means that R * Y and R^2 * Y are exactly representable.
	 */
	rsqrtps	%xmm11, %xmm7
	subps	%xmm11, %xmm9
	andps	%xmm5, %xmm7
	movaps	%xmm2, %xmm4

	/*
	 * Compute S = (Y/sqrt(Y + W)) * (1 + d)
	 * and T = (W/sqrt(Y + W)) * (1 + d)
	 * so that S + T = sqrt(Y + W) * (1 + d)
	 * S is exact, and the rounding error in T is OK.
	 */
	mulps	%xmm7, %xmm11
	movaps	%xmm7, %xmm6
	mulps	%xmm7, %xmm9
	mulps	%xmm11, %xmm6
	mulps	%xmm9, %xmm7

	/*
	 * For low-accuracy versions, the computation can be done
	 * just as U + ((S + T) + (S + T) * Corr)
	 */
	addps	%xmm9, %xmm11
	subps	%xmm6, %xmm10
	movaps	%xmm2, %xmm9
	subps	%xmm7, %xmm10
	mulps	%xmm10, %xmm8

	/* Now multiplex to the case X = 2^-30 * input, Xl = 0 in the "big" case. */
	movups	XScale+__svml_sacosh_data_internal(%rip), %xmm14
	mulps	%xmm0, %xmm14
	addps	sC2+__svml_sacosh_data_internal(%rip), %xmm8
	mulps	%xmm10, %xmm8
	andnps	%xmm14, %xmm3

	/*
	 * Now resume the main code.
	 * reduction: compute r, n
	 */
	movdqu	iBrkValue+__svml_sacosh_data_internal(%rip), %xmm14
	movdqu	iOffExpoMask+__svml_sacosh_data_internal(%rip), %xmm5

	/* Add 31 to the exponent in the "large" case to get log(2 * input) */
	movups	sThirtyOne+__svml_sacosh_data_internal(%rip), %xmm6
	addps	sHalf+__svml_sacosh_data_internal(%rip), %xmm8
	mulps	%xmm8, %xmm10
	movaps	%xmm1, %xmm8
	mulps	%xmm11, %xmm10
	addps	%xmm10, %xmm11
	addps	%xmm11, %xmm12
	maxps	%xmm12, %xmm13
	minps	%xmm12, %xmm9
	movaps	%xmm13, %xmm15
	addps	%xmm9, %xmm15
	subps	%xmm15, %xmm13
	andps	%xmm1, %xmm15
	orps	%xmm15, %xmm3
	addps	%xmm13, %xmm9
	psubd	%xmm14, %xmm3
	andps	%xmm1, %xmm9
	pand	%xmm3, %xmm5
	psrad	$23, %xmm3
	cvtdq2ps %xmm3, %xmm7
	pslld	$23, %xmm3
	paddd	%xmm14, %xmm5
	psubd	%xmm3, %xmm4

	/* polynomial evaluation */
	subps	%xmm2, %xmm5
	mulps	%xmm4, %xmm9
	addps	%xmm7, %xmm6
	movups	sPoly+112+__svml_sacosh_data_internal(%rip), %xmm2
	andnps	%xmm6, %xmm8
	andps	%xmm1, %xmm7
	addps	%xmm5, %xmm9
	mulps	%xmm9, %xmm2
	orps	%xmm7, %xmm8

	/* final reconstruction */
	mulps	sLn2+__svml_sacosh_data_internal(%rip), %xmm8
	addps	sPoly+96+__svml_sacosh_data_internal(%rip), %xmm2
	mulps	%xmm9, %xmm2
	addps	sPoly+80+__svml_sacosh_data_internal(%rip), %xmm2
	mulps	%xmm9, %xmm2
	addps	sPoly+64+__svml_sacosh_data_internal(%rip), %xmm2
	mulps	%xmm9, %xmm2
	addps	sPoly+48+__svml_sacosh_data_internal(%rip), %xmm2
	mulps	%xmm9, %xmm2
	addps	sPoly+32+__svml_sacosh_data_internal(%rip), %xmm2
	mulps	%xmm9, %xmm2
	addps	sPoly+16+__svml_sacosh_data_internal(%rip), %xmm2
	mulps	%xmm9, %xmm2
	addps	sPoly+__svml_sacosh_data_internal(%rip), %xmm2
	mulps	%xmm9, %xmm2
	mulps	%xmm9, %xmm2
	addps	%xmm2, %xmm9
	addps	%xmm8, %xmm9
	testl	%edx, %edx

	/* Go to special inputs processing branch */
	jne	L(SPECIAL_VALUES_BRANCH)
	# LOE rbx rbp r12 r13 r14 r15 edx xmm0 xmm9

	/* Restore registers
	 * and exit the function
	 */

L(EXIT):
	movaps	%xmm9, %xmm0
	addq	$72, %rsp
	cfi_def_cfa_offset(8)
	ret
	cfi_def_cfa_offset(80)

	/* Branch to process
	 * special inputs
	 */

L(SPECIAL_VALUES_BRANCH):
	movups	%xmm0, 32(%rsp)
	movups	%xmm9, 48(%rsp)
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
	movups	48(%rsp), %xmm9

	/* Go to exit */
	jmp	L(EXIT)
	cfi_offset(12, -64)
	cfi_offset(13, -72)
	cfi_offset(14, -80)
	# LOE rbx rbp r12 r13 r14 r15 xmm9

	/* Scalar math function call
	 * to process special input
	 */

L(SCALAR_MATH_CALL):
	movl	%r12d, %r14d
	movss	32(%rsp, %r14, 4), %xmm0
	call	acoshf@PLT
	# LOE rbx rbp r14 r15 r12d r13d xmm0

	movss	%xmm0, 48(%rsp, %r14, 4)

	/* Process special inputs in loop */
	jmp	L(SPECIAL_VALUES_LOOP)
	# LOE rbx rbp r15 r12d r13d
END(_ZGVbN4v_acoshf_sse4)

	.section .rodata, "a"
	.align	16

#ifdef __svml_sacosh_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(16)) VUINT32 sOne[4][1];
	__declspec(align(16)) VUINT32 sPoly[8][4][1];
	__declspec(align(16)) VUINT32 iBrkValue[4][1];
	__declspec(align(16)) VUINT32 iOffExpoMask[4][1];
	__declspec(align(16)) VUINT32 sBigThreshold[4][1];
	__declspec(align(16)) VUINT32 sC2[4][1];
	__declspec(align(16)) VUINT32 sC3[4][1];
	__declspec(align(16)) VUINT32 sHalf[4][1];
	__declspec(align(16)) VUINT32 sLargestFinite[4][1];
	__declspec(align(16)) VUINT32 sThirtyOne[4][1];
	__declspec(align(16)) VUINT32 sTopMask8[4][1];
	__declspec(align(16)) VUINT32 XScale[4][1];
	__declspec(align(16)) VUINT32 sLn2[4][1];
} __svml_sacosh_data_internal;
#endif
__svml_sacosh_data_internal:
	/* sOne = SP 1.0 */
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
	/* iBrkValue = SP 2/3 */
	.align	16
	.long	0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab, 0x3f2aaaab
	/* iOffExpoMask = SP significand mask */
	.align	16
	.long	0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff
	/* sBigThreshold */
	.align	16
	.long	0x4E800000, 0x4E800000, 0x4E800000, 0x4E800000
	/* sC2 */
	.align	16
	.long	0x3EC00000, 0x3EC00000, 0x3EC00000, 0x3EC00000
	/* sC3 */
	.align	16
	.long	0x3EA00000, 0x3EA00000, 0x3EA00000, 0x3EA00000
	/* sHalf */
	.align	16
	.long	0x3F000000, 0x3F000000, 0x3F000000, 0x3F000000
	/* sLargestFinite */
	.align	16
	.long	0x7F7FFFFF, 0x7F7FFFFF, 0x7F7FFFFF, 0x7F7FFFFF
	/* sThirtyOne */
	.align	16
	.long	0x41F80000, 0x41F80000, 0x41F80000, 0x41F80000
	/* sTopMask8 */
	.align	16
	.long	0xFFFF0000, 0xFFFF0000, 0xFFFF0000, 0xFFFF0000
	/* XScale */
	.align	16
	.long	0x30800000, 0x30800000, 0x30800000, 0x30800000
	/* sLn2 = SP ln(2) */
	.align	16
	.long	0x3f317218, 0x3f317218, 0x3f317218, 0x3f317218
	.align	16
	.type	__svml_sacosh_data_internal, @object
	.size	__svml_sacosh_data_internal, .-__svml_sacosh_data_internal
